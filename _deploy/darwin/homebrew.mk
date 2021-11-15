# Created by: Marcin Konarski <amok@codestation.org>
# huginn (R) 2021 Copyright codestation.org all rights reserved.
# NATIVE_MAKE

VERIFY_DIR=../../huginnrc
VERSION=$(shell awk '/^VERSION *=|SUBVERSION *=|EXTRAVERSION *=/{VERSION=VERSION DOT $$3;DOT="."}END{print VERSION}' ../../Makefile.mk.in)
CODENAME=$(shell sw_vers -productVersion | sed -e 's/^12[.].*/monterey/' -e 's/^11[.].*/big_sur/' -e 's/^10[.].*/catalina/')
PWD=$(shell pwd)
ifneq ($(BUILD_ID),)
REVISION=$(BUILD_ID)
REVISION_SEPARATOR=-
endif
NAME=huginn
FORMULAE_DIR=homebrew
FORMULAE=./$(NAME).rb
ARTIFACT_STEM=$(NAME)--$(VERSION)$(REVISION_SEPARATOR)$(REVISION).$(CODENAME).bottle
ARTIFACT_RAW=$(ARTIFACT_STEM).tar.gz
ARTIFACT=$(subst --,-,$(ARTIFACT_RAW))
SYSTEM=$(shell uname -s | tr A-Z a-z)
SYSTEM_RELEASE=$(shell uname -r | tr A-Z a-z)
ARCH=$(shell uname -m)
BUNDLE_DIR=$(NAME)-bundle/$(SYSTEM)-$(SYSTEM_RELEASE)/$(ARCH)/$(VERSION)
BUNDLE=$(NAME)-$(VERSION)-$(SYSTEM)-$(SYSTEM_RELEASE)-$(ARCH).tar
BUNDLE_WRAP=$(NAME)-bundle.tar

.PHONY: all package bundle clean do-package

all: $(VERIFY_DIR)
	@echo "Explicit target required!" && echo && echo "supported targets: clean package bundle"

$(VERIFY_DIR):
	@echo "You must run this Makefile from $(NAME)/_deploy/darwin directory." && false

$(FORMULAE_DIR)/$(FORMULAE): $(VERIFY_DIR) formulae.rb.in
	@mkdir -p $(FORMULAE_DIR) \
	&& sed -e 's/@version@/$(VERSION)/g' -e 's/@revision@/$(REVISION_SEPARATOR)$(REVISION)/g' -e 's:@repo@:$(PWD)/../..:g' formulae.rb.in > $(@)

$(FORMULAE_DIR)/$(ARTIFACT_RAW): $(VERIFY_DIR) $(FORMULAE_DIR)/$(FORMULAE)
	@cd $(FORMULAE_DIR) \
	&& brew install --build-bottle --fetch-HEAD --force -v --formula $(FORMULAE) \
	&& brew bottle --force-core-tap --root-url https://codestation.org/darwin/ --json $(FORMULAE) --no-rebuild \
	&& brew bottle --force-core-tap --root-url https://codestation.org/darwin/ --merge $(ARTIFACT_STEM).json --write --no-commit

$(FORMULAE_DIR)/$(ARTIFACT): $(VERIFY_DIR) $(FORMULAE_DIR)/$(ARTIFACT_RAW)
	@cp $(FORMULAE_DIR)/$(ARTIFACT_RAW) $(@)

do-package: $(VERIFY_DIR) $(FORMULAE_DIR)/$(ARTIFACT)

package:
	@$(MAKE) -f homebrew.mk do-package

bundle: $(VERIFY_DIR) $(FORMULAE_DIR)/$(BUNDLE_WRAP)

$(FORMULAE_DIR)/$(BUNDLE_WRAP): $(VERIFY_DIR) $(FORMULAE_DIR)/$(BUNDLE)
	@cd $(FORMULAE_DIR) && tar cf $(BUNDLE_WRAP) $(BUNDLE)

$(FORMULAE_DIR)/$(BUNDLE): $(VERIFY_DIR) $(FORMULAE_DIR)/$(ARTIFACT)
	@mkdir -p $(FORMULAE_DIR)/$(BUNDLE_DIR) \
	&& cp -f $(FORMULAE_DIR)/$(ARTIFACT) $(FORMULAE_DIR)/$(BUNDLE_DIR) \
	&& cd $(FORMULAE_DIR) \
	&& tar cf $(BUNDLE) $(NAME)-bundle $(FORMULAE)

clean: $(VERIFY_DIR) $(FORMULAE_DIR)/$(FORMULAE)
	@cd $(FORMULAE_DIR) \
	&& brew uninstall --zap --force --ignore-dependencies $(FORMULAE) ;\
	cd - \
	&& /bin/rm -rf $(FORMULAE_DIR)

