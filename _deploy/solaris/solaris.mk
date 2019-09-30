VERIFY_DIR=../../huginnrc
PKGFILENAME=huginn-$(VERSION)-solaris$(OS)-$(ARCH).pkg
PREFIX=/usr/local
SYSCONFDIR=/etc
LOCALSTATEDIR=/var
DESTDIR=$(VPATH)/pkg
MAKE_ENV=PREFIX=$(PREFIX) SYSCONFDIR=$(SYSCONFDIR) LOCALSTATEDIR=$(LOCALSTATEDIR) DESTDIR=$(DESTDIR) CONFIGURE="--enable-auto-sanity"
ARTIFACT=$(DESTDIR)/$(PREFIX)/lib/libhdata.so
BUNDLE_DIR=huginn-bundle/solaris-$(OS)/$(ARCH)/$(PORTVERSION)
BUNDLE=huginn-$(VERSION)-solaris-$(OS)-$(ARCH).tar
BUNDLE_WRAP=huginn-bundle.tar
TEST_ARTIFACT=$(DESTDIR)/test-ok
CPLUS_INCLUDE_PATH:=$(CPLUS_INCLUDE_PATH):$(BASEDIR)/usr/local/include
LIBRARY_PATH:=$(LIBRARY_PATH):$(BASEDIR)/usr/local/lib
LD_LIBRARY_PATH:=$(LD_LIBRARY_PATH):$(BASEDIR)/usr/local/lib

all: $(VERIFY_DIR)
	@echo "Explicit target required!" && echo && echo "supported targets: clean package bundle"

$(VERIFY_DIR):
	@echo "You must run this Makefile from huginn/_deploy/solaris directory." && false

$(ARTIFACT): $(VERIFY_DIR)
	mkdir -p pkg && \
	echo "BASEDIR=$(BASEDIR)" && \
	cd ../../ && $(MAKE) $(MAKE_ENV) debug release && \
	mkdir -p build/doc && touch build/doc/huginn.1 && \
	$(MAKE) $(MAKE_ENV) install-debug install-release

$(TEST_ARTIFACT): $(ARTIFACT)
	cd ../../ && $(MAKE) $(MAKE_ENV) test && touch $(@)

$(PKGFILENAME): $(TEST_ARTIFACT) pkginfo.in solaris.mk GNUmakefile makefile
	@echo "i pkginfo" > Prototype && \
	pkgproto pkg= | awk '{print $$1" "$$2" "$$3" "$$4" root root"}' | sed -e 's/ 0700 / 0755 /' >> Prototype && \
	sed -e 's/@VERSION@/$(VERSION)/' -e 's/@ARCH@/$(ARCH)/' pkginfo.in > pkginfo && \
	pkgmk -o -r . -d . -f Prototype && \
	pkgtrans -s ./ $(PKGFILENAME) huginn && \
	/bin/rm -rf huginn

package: $(PKGFILENAME)

clean: $(VERIFY_DIR)
	@/bin/rm -rf pkg tmp huginn-*-solaris*-*.pkg pkginfo Prototype && \
	cd ../../ && $(MAKE) purge

tmp/$(BUNDLE): $(PKGFILENAME)
	@mkdir -p tmp && cd tmp && \
	/bin/rm -rf huginn-bundle && \
	mkdir -p $(BUNDLE_DIR) && \
	cp ../$(PKGFILENAME) $(BUNDLE_DIR) && \
	tar cf $(BUNDLE) huginn-bundle

tmp/$(BUNDLE_WRAP): tmp/$(BUNDLE)
	@cd tmp && \
	tar cf $(BUNDLE_WRAP) $(BUNDLE)

bundle: tmp/$(BUNDLE_WRAP)

