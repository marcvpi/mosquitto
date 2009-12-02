include config.mk

DIRS=man src
DISTDIRS=man

.PHONY : all mosquitto clean reallyclean install uninstall dist distclean sign copy

all : mosquitto

mosquitto :
	for d in ${DIRS}; do $(MAKE) -C $${d}; done

clean :
	for d in ${DIRS}; do $(MAKE) -C $${d} clean; done

reallyclean : 
	for d in ${DIRS}; do $(MAKE) -C $${d} reallyclean; done
	-rm -f *.orig

install : mosquitto
	@for d in ${DIRS}; do $(MAKE) -C $${d} install; done

uninstall :
	@for d in ${DIRS}; do $(MAKE) -C $${d} uninstall; done

dist : reallyclean
	@for d in ${DISTDIRS}; do $(MAKE) -C $${d} dist; done
	
	mkdir -p dist/mosquitto-${VERSION}
	cp -r logo man src windows COPYING Makefile config.mk readme.txt dist/mosquitto-${VERSION}/
	cd dist; tar -zcf mosquitto-${VERSION}.tar.gz mosquitto-${VERSION}/

distclean : clean
	@for d in ${DISTDIRS}; do $(MAKE) -C $${d} distclean; done
	
	-rm -rf dist/

sign : dist
	cd dist; gpg --detach-sign -a mosquitto-${VERSION}.tar.gz

copy : sign
	man2html man/mosquitto.1 > dist/mosquitto.html
	cd dist; scp mosquitto-${VERSION}.tar.gz mosquitto-${VERSION}.tar.gz.asc atchoo:mosquitto.atchoo.org/files/
	cd dist; scp *.html atchoo:mosquitto.atchoo.org/man/

