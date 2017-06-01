#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <monitors/monitor.h>
#include <logging/logging.h>
#include <errno.h>
#include <monitors/dbus_monitor.h>
#include "errors.h"
#include <gio/gio.h>

#define INTERFACES_ADDED_SIGNAL 	"InterfacesAdded"
#define INTERFACES_REMOVED_SIGNAL 	"InterfacesRemoved"
#define UDISKS_SERVICE_NAME		 	"org.freedesktop.DBus.ObjectManager"
#define UDISKS_OBJECT_PATH		 	"/org/freedesktop/UDisks2"
#define UDISKS_DRIVER_OBJECT_PATH 	"/org/freedesktop/UDisks2/drives/"
#define NM_SERVICE_NAME		 		"org.freedesktop.NetworkManager"
#define NM_OBJECT_PATH		 		"/org/freedesktop/NetworkManager"
#define NM_STATE_CHANGED_SIGNAL		"StateChanged"

static void* monitoring_thread(void* dbus_monitor_ptr);
static void mark_dead(monitor_t monitor);

int dbus_monitor_from_args(int argc, char* argv[], monitor_t* monitor) {
	if (argc > 1) return E_INVALID_MONITOR_ARGUMENT;
	*monitor = (monitor_t)malloc(sizeof(struct monitor_t));
	if (*monitor == NULL) {
		log_error("malloc: %s", strerror(errno));
		return E_OUT_OF_MEMORY;
	}
	(*monitor)->type = MONITOR_TYPE_DBUS;

	(*monitor)->dbus = (dbus_monitor_t)malloc(sizeof(struct dbus_monitor));
	if ((*monitor)->dbus == NULL) {
		log_error("malloc: %s", strerror(errno));
		free(monitor);
		return E_OUT_OF_MEMORY;
	}
	memset((*monitor)->dbus, '\0', sizeof(struct dbus_monitor));
	dbus_monitor_t dbus_monitor = (*monitor)->dbus;

	if (pthread_mutex_init(&(dbus_monitor->state_mutex), NULL) != 0) {
		free(dbus_monitor);
		free((*monitor));
		return CALL_FAILURE;
	};

	if(strcmp(argv[0], "--disks") == 0) {
		dbus_monitor->type = DBUS_MONITOR_TYPE_UDISKS;
	} else if(strcmp(argv[0], "--network") == 0) {
		dbus_monitor->type = DBUS_MONITOR_TYPE_NM;
	} else {
		log_error("unknow dbus monitor type %s", argv[0]);
		free(dbus_monitor);
		free((*monitor));
		return E_INVALID_MONITOR_ARGUMENT;
	}

	pthread_mutex_lock(&(dbus_monitor->state_mutex));
	(*monitor)->state = MONITOR_STATE_INITIALIZED;
	pthread_mutex_unlock(&(dbus_monitor->state_mutex));
	return CALL_SUCCESS;
}

int dbus_start(monitor_t monitor) {
	pthread_mutex_lock(&(monitor->dbus->state_mutex));
	if (monitor->state != MONITOR_STATE_INITIALIZED) {
		log_error("cannot start monitor wich is not in \'initialized\' state");
		pthread_mutex_unlock(&(monitor->dbus->state_mutex));
		return E_MONITOR_INVALID_STATE;
	}
	if (pthread_create(&(monitor->dbus->thread),
					   NULL, monitoring_thread, monitor) != 0) {
		log_error("dbus pthread_create: %s", strerror(errno));
		pthread_mutex_unlock(&(monitor->dbus->state_mutex));
		return CALL_FAILURE;
	}
	monitor->state = MONITOR_STATE_RUNNING;
	pthread_mutex_unlock(&(monitor->dbus->state_mutex));
	return CALL_SUCCESS;
}

int dbus_stop(monitor_t monitor) {
	pthread_mutex_lock(&(monitor->dbus->state_mutex));
	if (monitor->state != MONITOR_STATE_RUNNING) {
		pthread_mutex_unlock(&(monitor->dbus->state_mutex));
		return E_MONITOR_INVALID_STATE;
	}
	monitor->state = MONITOR_STATE_DYING;
	g_main_loop_quit(monitor->dbus->subscription_loop);
	pthread_mutex_unlock(&(monitor->dbus->state_mutex));
	return CALL_SUCCESS;
}

void dbus_join(monitor_t monitor) {
	pthread_join(monitor->dbus->thread, NULL);
	log_info("dbus monitor was stopped");
}

void dbus_print_usage(int type) {
	if (type == DBUS_MONITOR_TYPE_UDISKS) {
		printf("%s%s",
			   "Aimed to monitor disks events (disk added/removed)\n",
			   "Usage: slm --disks\n");
	} else {
		printf("%s%s",
			   "Aimed to monitor network events (networking stage changes)\n",
			   "Usage: slm --network\n");
	}
}

int dbus_monitor_destroy(monitor_t monitor) {
	if (monitor->state != MONITOR_STATE_DEAD) {
		return E_MONITOR_INVALID_STATE;
	}
	log_info("dbus monitor was gracefuly destroyed");
	dbus_monitor_t dbus_monitor = monitor->dbus;
	free(dbus_monitor);
	free(monitor);
	return CALL_SUCCESS;
}

typedef struct dbus_callback_parameters {
	GDBusConnection* connection;
	int* monitor_type;
} dbus_callback_parameters;

static int get_dbus_string_property(GDBusConnection* connection,
									const char* service,
									const char* obj_path,
									const char* prop_interface,
									const char* prop_name,
									char** return_value) {
	GError* error = NULL;
	GVariant* parameter = g_dbus_connection_call_sync (connection,
					service,
					obj_path,
					"org.freedesktop.DBus.Properties",
					"Get",
					g_variant_new ("(ss)", prop_interface, prop_name),
					G_VARIANT_TYPE ("(v)"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	if (parameter == NULL) {
		log_error("Error reading property \'%s\' of \'%s\': %s\n", prop_name,
				  prop_interface, error->message);
		g_error_free (error);
		return CALL_FAILURE;
	}
	GVariant* tmp;
	g_variant_get (parameter, "(v)", &tmp);
	g_variant_get (tmp, "s", return_value);
	return CALL_SUCCESS;
}

static void udisks_callback (GDBusConnection *connection,
					  const gchar* sender_name,
					  const gchar* object_path,
					  const gchar* interface_name,
					  const gchar* signal_name,
					  GVariant* parameters,
					  gpointer user_data) {
	dbus_callback_parameters* cp = (dbus_callback_parameters*)user_data;
	if (*(cp->monitor_type) == DBUS_MONITOR_TYPE_UDISKS) {
		if (strcmp(signal_name, INTERFACES_ADDED_SIGNAL) == 0) {
			const gchar* new_interface_object_path;
			g_variant_get (parameters, "(oa{sa{sv}})", &new_interface_object_path, NULL);
			if (new_interface_object_path - strstr(new_interface_object_path,
								 UDISKS_DRIVER_OBJECT_PATH) == 0) {
				gchar* disk_model_s;
				gchar* disk_connection_bus_s;
				int call_result = get_dbus_string_property(connection,
						"org.freedesktop.UDisks2",
						new_interface_object_path,
						"org.freedesktop.UDisks2.Drive",
						"Model", &disk_model_s);
				if (call_result != CALL_SUCCESS) {
					return;
				}
				call_result = get_dbus_string_property(connection,
										 "org.freedesktop.UDisks2",
										 new_interface_object_path,
										 "org.freedesktop.UDisks2.Drive",
										 "ConnectionBus", &disk_connection_bus_s);
				if (call_result != CALL_SUCCESS) {
					return;
				}
				log_info("Disk \'%s\' has been connected via %s",
						 disk_model_s, disk_connection_bus_s);
				g_free(disk_connection_bus_s);
				g_free(disk_model_s);
			}
		} else if (strcmp(signal_name, INTERFACES_REMOVED_SIGNAL) == 0) {
			const gchar *old_interface_object_path;
			g_variant_get(parameters, "(oas)", &old_interface_object_path, NULL);
			if (old_interface_object_path - strstr(old_interface_object_path,
												   UDISKS_DRIVER_OBJECT_PATH) == 0) {
				log_info("Disk \'%s\' removed", old_interface_object_path);
			}
		}
	} else if (*(cp->monitor_type) == DBUS_MONITOR_TYPE_NM) {
		if (strcmp(signal_name, NM_STATE_CHANGED_SIGNAL) == 0) {
			const guint new_state;
			g_variant_get(parameters, "(u)", &new_state);
			switch (new_state) {
				case 10 : {
					log_info("networking disabled");
					break;
				}
				case 20 : {
					log_info("networking enabled, no active connection");
					break;
				}
				case 50 : {
					log_info("local connection enbaled");
					break;
				}
				case 70 : {
					log_info("global connection enbaled");
					break;
				}
				default: {
//					log_info("NM device state changed to %u",  new_state);
				};
			}

		}
	}
}

static void* monitoring_thread(void* monitor_ptr) {
	log_info("dbus monitor was created");

	monitor_t monitor = (monitor_t)monitor_ptr;
	dbus_monitor_t dbus_monitor = monitor->dbus;

	sigset_t blocking_mask;
	if (sigfillset(&blocking_mask) != 0) {
		log_error("cannot create sigmask for dbus thread, exit");
		pthread_mutex_lock(&(dbus_monitor->state_mutex));
		monitor->state = MONITOR_STATE_DEAD;
		pthread_mutex_unlock(&(dbus_monitor->state_mutex));
		return NULL;
	}
	if (pthread_sigmask(SIG_BLOCK, &blocking_mask, NULL) != 0) {
		log_error("cannot mask dbus signals, exit");
		pthread_mutex_lock(&(dbus_monitor->state_mutex));
		monitor->state = MONITOR_STATE_DEAD;
		pthread_mutex_unlock(&(dbus_monitor->state_mutex));
		return NULL;
	}

	GError *error = NULL;
	GDBusConnection* connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (connection == NULL) {
		g_printerr ("Error connecting to D-Bus address: %s\n", error->message);
		g_error_free (error);
		return NULL;
	}

	const char* interface_added_method;
	const char* interface_removed_method;
	const char* object_path;
	const char* service_name;

	dbus_callback_parameters cp = {
			connection,
			&(dbus_monitor->type)
	};

	guint add_subscription_id = 0;
	guint remove_subscription_id = 0;

	if(dbus_monitor->type == DBUS_MONITOR_TYPE_UDISKS) {
		interface_added_method = INTERFACES_ADDED_SIGNAL;
		interface_removed_method = INTERFACES_REMOVED_SIGNAL;
		object_path = UDISKS_OBJECT_PATH;
		service_name = UDISKS_SERVICE_NAME;

		add_subscription_id = g_dbus_connection_signal_subscribe
				(connection,
				 NULL,
				 service_name,
				 interface_added_method,
				 object_path,
				 NULL,
				 G_DBUS_SIGNAL_FLAGS_NONE,
				 udisks_callback,
				 &cp,
				 NULL);
		remove_subscription_id = g_dbus_connection_signal_subscribe
				(connection,
				 NULL,
				 service_name,
				 interface_removed_method,
				 object_path,
				 NULL,
				 G_DBUS_SIGNAL_FLAGS_NONE,
				 udisks_callback,
				 &cp,
				 NULL);
	} else if(dbus_monitor->type == DBUS_MONITOR_TYPE_NM) {
		interface_added_method = NM_STATE_CHANGED_SIGNAL;//NM_DEVICE_ADDED_SIGNAL;
		object_path = NM_OBJECT_PATH;
		service_name = NM_SERVICE_NAME;

		add_subscription_id = g_dbus_connection_signal_subscribe
				(connection,
				 NULL,
				 service_name,
				 interface_added_method,
				 object_path,
				 NULL,
				 G_DBUS_SIGNAL_FLAGS_NONE,
				 udisks_callback,
				 &cp,
				 NULL);
	} else {
		log_error("unexpected dbus monitor type: %d", dbus_monitor->type);
		g_object_unref (connection);
		pthread_mutex_lock(&(dbus_monitor->state_mutex));
		monitor->state = MONITOR_STATE_DEAD;
		pthread_mutex_unlock(&(dbus_monitor->state_mutex));
		return NULL;
	}

	dbus_monitor->subscription_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(dbus_monitor->subscription_loop);
	if(dbus_monitor->type == DBUS_MONITOR_TYPE_UDISKS) {
		g_dbus_connection_signal_unsubscribe(connection, add_subscription_id);
		g_dbus_connection_signal_unsubscribe(connection, remove_subscription_id);
	} else {
		g_dbus_connection_signal_unsubscribe(connection, add_subscription_id);
	}

	g_main_loop_unref(dbus_monitor->subscription_loop);
	g_object_unref (connection);

	pthread_mutex_lock(&(dbus_monitor->state_mutex));
	monitor->state = MONITOR_STATE_DEAD;
	pthread_mutex_unlock(&(dbus_monitor->state_mutex));
	pthread_exit(NULL);
	return NULL;
}
