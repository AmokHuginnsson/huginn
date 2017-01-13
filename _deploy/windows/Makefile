# NATIVE_MAKE

SHELL     = /bin/sh
CPACK=$(wildcard /cygdrive/c/Program\ Files\ (x86)/CMake*/bin/cpack.exe)$(wildcard /cygdrive/c/Program\ Files/CMake*/bin/cpack.exe)
UNQUOTE   = $(subst $\",,$(1))
VERIFY_DIR=../../huginnrc
VERSION=$(shell awk '/^VERSION *=|SUBVERSION *=|EXTRAVERSION *=/{VERSION=VERSION DOT $$3;DOT="."}END{print VERSION}' ../../Makefile.mk.in)
PACKAGE_VERSION=$(or $(call UNQUOTE,$(BUILD_ID)),1)
ARCH=$(shell arch)
SYSTEM=windows
TAG=$(VERSION)-$(PACKAGE_VERSION)-win32
SYSTEM_RELEASE=$(shell lsb_release -sr|awk -F '.' '{print $$1}')
DEPLOY_DIR=_deploy/windows/msi
ARTIFACTS=msi/huginn-$(TAG).msi
BUNDLE_DIR=huginn-bundle/$(SYSTEM)/$(VERSION)
BUNDLE=huginn-$(VERSION)-$(SYSTEM).tar
BUNDLE_WRAP=huginn-bundle.tar

.PHONY: all package bundle clean

all: $(VERIFY_DIR)
	@echo "Explicit target required!" && echo && echo "supported targets: clean package bundle"

$(VERIFY_DIR):
	@echo "You must run this Makefile from huginn/_deploy/windows directory." && false

$(ARTIFACTS): $(VERIFY_DIR)
	@cd ../../ && \
	make -f msvcxx.mk EXTRA_FLAGS=BUILD_PACKAGE=1 release && \
	mkdir -p $(DEPLOY_DIR) && \
	cd build/release && \
	"$(CPACK)" -B `cygpath -wa ../../$(DEPLOY_DIR)` && \
	cd - && \
	mv -f $(DEPLOY_DIR)/huginn-$(VERSION)-win32.msi $(DEPLOY_DIR)/../$(@)

msi/$(BUNDLE): $(VERIFY_DIR) $(ARTIFACTS)
	@/bin/rm -rf msi/huginn-bundle && \
	mkdir -p msi/$(BUNDLE_DIR) && \
	cp $(ARTIFACTS) msi/$(BUNDLE_DIR) && \
	cd msi && tar cf $(BUNDLE) huginn-bundle

msi/$(BUNDLE_WRAP): $(VERIFY_DIR) msi/$(BUNDLE)
	@cd msi && tar cf $(BUNDLE_WRAP) $(BUNDLE)

package: $(VERIFY_DIR) $(ARTIFACTS)

bundle: $(VERIFY_DIR) msi/$(BUNDLE_WRAP)

clean: $(VERIFY_DIR)
	@/bin/rm -f $(BUNDLE_WRAP) $(BUNDLE) && \
	/bin/rm -rf msi huginn-bundle && \
	cd ../../ && \
	make purge

