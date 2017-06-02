# SLM
## About
SLM (stands for System Linux Monitor) is a Linux utility aimed to simplify OS kernel state analizing. SLM distribution comes as utility and daemon, both have equal functionality but different run modes. 

### Functionality
To see available commands, call `slm --help`. SLM can be used to monitor following events:
* file events (open/close/write/delete-move)
* bluetooth status (on/off)
* power status (on/off)
* network status
* inserting storage devices

## How to use
### slm utility
Utility write down monitoring logs in to console and can be used to monitor single event type.

### slmd daemon
Daemon should be managered by systemd. Configuration file is located in /etc/config/slmd.config. Configuration commands are same as for utility. Daemon output log file is located in /var/log/slmd.log.
 * `sudo systemctl start slmd`
 * `sudo systemctl stop slmd`
 * `sudo systemctl reload slmd`
 * `sudo systemctl status slmd` 

## How to build
You need to have CMake installed on your system to build slm. Also note that it depends on glib-2.0 and gio-2.0, udev, pthreads libraries.
1. clone this repo with 

        git clone https://github.com/ZoXaL/spovm4_slm
        
2. configure CMake with 
    
        cmake -H. -B_buidls
        
3. build

        cmake --build _builds
    
4. run 

        _builds/slm --help
        
5. if you have CPack (CMake module) installed, you can build binary debian package with
    
        cd _builds
        cpack ..
    
## Contribution
Feel free to send pull requests or propose any ideas.
