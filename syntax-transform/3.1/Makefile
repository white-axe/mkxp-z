SYNTAX_TRANSFORM_PATCHES := \
	$(SYNTAX_TRANSFORM_DIR)/0000-prelude.patch \
	$(SYNTAX_TRANSFORM_DIR)/0001-space-before-argument-parentheses.patch \
	$(SYNTAX_TRANSFORM_DIR)/0002-singleton-class-superclass.patch \
	$(SYNTAX_TRANSFORM_DIR)/0003-hash-args.patch \
	$(SYNTAX_TRANSFORM_DIR)/0004-block-variable-shadowing.patch \
	$(SYNTAX_TRANSFORM_DIR)/0005-then-do-colon.patch \
	$(SYNTAX_TRANSFORM_DIR)/0006-character-literal.patch \
	$(SYNTAX_TRANSFORM_DIR)/0007-multiple-values-for-a-block-parameter.patch \
	$(SYNTAX_TRANSFORM_DIR)/0008-else-without-rescue.patch \
	$(SYNTAX_TRANSFORM_DIR)/0009-module-constants.patch \
	$(SYNTAX_TRANSFORM_DIR)/0010-nil-true-false-constants.patch \
	$(SYNTAX_TRANSFORM_DIR)/0011-variables-array.patch \
	$(SYNTAX_TRANSFORM_DIR)/0012-hash-select.patch \
	$(SYNTAX_TRANSFORM_DIR)/0013-numeric-div.patch \
	$(SYNTAX_TRANSFORM_DIR)/0014-class-variable-access-from-toplevel.patch \
	$(SYNTAX_TRANSFORM_DIR)/0015-class-variable-overtaken.patch \

HAVE_SYNTAX_TRANSFORM ?= 1

syntax-transform: $(RUBY_SRC)/configure.ac $(SYNTAX_TRANSFORM_PATCHES)
ifeq ($(HAVE_SYNTAX_TRANSFORM),1)
ifeq ($(patsubst MINGW%,,$(firstword $(shell uname))),)
	cd $(RUBY_SRC) && for patch in $(SYNTAX_TRANSFORM_PATCHES); do echo "Applying $$patch"; patch -i $(PWD)/$$patch; done
else
	cd $(RUBY_SRC) && for patch in $(SYNTAX_TRANSFORM_PATCHES); do echo "Applying $$patch"; $(GIT) apply $(PWD)/$$patch; done
endif
endif
