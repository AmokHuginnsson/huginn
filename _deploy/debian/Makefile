# NATIVE_MAKE

.NOTPARALLEL:

SHELL     = /bin/sh
UNQUOTE   = $(subst $\",,$(1))
BUILDINFO = $(if $(shell dpkg-buildpackage --help | grep buildinfo-option),--buildinfo-option=-O)
VERIFY_DIR=../../huginnrc
OUTPUT_DIR=../../debian
CHANGELOG =$(OUTPUT_DIR)/changelog
VERSION=$(shell awk '/^VERSION *=|SUBVERSION *=|EXTRAVERSION *=/{VERSION=VERSION DOT $$3;DOT="."}END{print VERSION}' ../../Makefile.mk.in)
PACKAGE_VERSION=$(VERSION)$(call UNQUOTE,$(BUILD_ID))
ARCH=$(shell dpkg-architecture -qDEB_BUILD_ARCH)
SYSTEM=$(shell lsb_release -si | tr A-Z a-z)
SYSTEM_RELEASE=$(shell lsb_release -sr)
ARTIFACTS=huginn_$(PACKAGE_VERSION)_$(ARCH).deb \
					huginn_$(VERSION).dsc \
					huginn_$(VERSION).tar.gz \
					huginn_$(VERSION)_$(ARCH).changes
ARTIFACTS_TARGET=$(addprefix $(OUTPUT_DIR)/,$(ARTIFACTS))
BUNDLE_DIR=huginn-bundle/$(SYSTEM)-$(SYSTEM_RELEASE)/$(ARCH)/$(VERSION)
BUNDLE=huginn-$(VERSION)-$(SYSTEM)-$(SYSTEM_RELEASE)-$(ARCH).tar
BUNDLE_WRAP=huginn-bundle.tar
export PACKAGE_VERSION

unexport CINCLUDE_PATH
unexport CPLUSINCLUDE_PATH
unexport LIBRARY_PATH
unexport LD_LIBRARY_PATH

.PHONY: all package bundle clean

all: $(VERIFY_DIR)
	@echo "Explicit target required!" && echo && echo "supported targets: clean package bundle"

$(VERIFY_DIR):
	@echo "You must run this Makefile from huginn/_deploy/debian directory." && false

$(CHANGELOG): $(VERIFY_DIR)
	@/bin/rm -f $(CHANGELOG) && \
	cd ../../ && \
	if [ ! -d debian ] ; then cp -a ./_deploy/debian . ; fi && \
	./_aux/mkchangelog debian/changelog

$(ARTIFACTS_TARGET): $(VERIFY_DIR) $(CHANGELOG)
	@cd ../../ && \
	dpkg-buildpackage -Zgzip -us -uc -F --changes-option=-Dversion=$(PACKAGE_VERSION) $(BUILDINFO) && \
	mv $(addprefix ../,$(ARTIFACTS)) debian/

$(OUTPUT_DIR)/tmp/$(BUNDLE): $(VERIFY_DIR) $(ARTIFACTS_TARGET)
	@/bin/rm -rf tmp/huginn-bundle && \
	mkdir -p $(OUTPUT_DIR)/tmp/$(BUNDLE_DIR) && \
	cp $(ARTIFACTS_TARGET) $(OUTPUT_DIR)/tmp/$(BUNDLE_DIR) && \
	cd $(OUTPUT_DIR)/tmp && tar cf $(BUNDLE) huginn-bundle

$(OUTPUT_DIR)/tmp/$(BUNDLE_WRAP): $(VERIFY_DIR) $(OUTPUT_DIR)/tmp/$(BUNDLE)
	@cd $(OUTPUT_DIR)/tmp && tar cf $(BUNDLE_WRAP) $(BUNDLE)

package: $(VERIFY_DIR) $(ARTIFACTS_TARGET)

bundle: $(VERIFY_DIR) $(OUTPUT_DIR)/tmp/$(BUNDLE_WRAP)

clean: $(VERIFY_DIR)
	cd ../../ && \
	if [ ! -d debian ] ; then cp -a ./_deploy/debian . ; fi && \
	/bin/rm -f debian/changelog && \
	dch --force-distribution --distribution any --create --newversion 0.0.0 --package huginn cleaner && \
	dpkg-buildpackage -Tclean && \
	/bin/rm -r debian

