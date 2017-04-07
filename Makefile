buildFolder:=build
srcFolder:=src

srcFilesPattern := ${srcFolder}/*.c ${srcFolder}/*/*.c
# all source files
srcFiles:=${wildcard ${srcFilesPattern}}

# all object files
osrcFiles:=${patsubst %.c,%.o,${srcFiles}}
osrcFiles:=${subst ${srcFolder},${buildFolder},${osrcFiles}}

CFLAGS:=-Wall -pedantic -std=c11 -g -Iinclude -Iinclude/modules

.PHONY: run

${buildFolder}/slm-debug: ${osrcFiles}
	@echo building $@ ...
	@gcc ${CFLAGS} -o $@ $^

${buildFolder}/slm: ${buildFolder}/slm-debug
	@echo building $@ ...
	@strip -d -o $@ $<

${buildFolder}/%.o: ${srcFolder}/%.c
	@echo building $@ ...
	@mkdir -p ${dir ${subst ${srcFolder},${buildFolder},$@}}
	@gcc ${CFLAGS} -c -o ${subst ${srcFolder},${buildFolder},$@} $<

run:
	@cd ${buildFolder}&&./slm-debug

p:
	@echo ${osrcFiles}