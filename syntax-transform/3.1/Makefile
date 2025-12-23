SYNTAX_TRANSFORM_PATCHES := \
	$(SYNTAX_TRANSFORM_DIR)/0000-prelude.patch \
	$(SYNTAX_TRANSFORM_DIR)/0001-space-before-argument-parentheses.patch \
	$(SYNTAX_TRANSFORM_DIR)/0002-hash-args.patch \
	$(SYNTAX_TRANSFORM_DIR)/0003-then-do-colon.patch \
	$(SYNTAX_TRANSFORM_DIR)/0004-character-literal.patch \
	$(SYNTAX_TRANSFORM_DIR)/0005-else-without-rescue.patch \
	$(SYNTAX_TRANSFORM_DIR)/0006-block-variable-shadowing.patch \
	$(SYNTAX_TRANSFORM_DIR)/0007-variables-array.patch \
	$(SYNTAX_TRANSFORM_DIR)/0008-class-variable-overtaken.patch \
	$(SYNTAX_TRANSFORM_DIR)/0009-nil-true-false-constants.patch \
	$(SYNTAX_TRANSFORM_DIR)/0010-singleton-class-superclass.patch \
	$(SYNTAX_TRANSFORM_DIR)/0011-hash-select.patch \
	$(SYNTAX_TRANSFORM_DIR)/0012-multiple-values-for-a-block-parameter.patch \
	$(SYNTAX_TRANSFORM_DIR)/0013-class-variable-access-from-toplevel.patch \
	$(SYNTAX_TRANSFORM_DIR)/0014-module-constants.patch \
	$(SYNTAX_TRANSFORM_DIR)/0015-numeric-div.patch \

HAVE_SYNTAX_TRANSFORM ?= 1

syntax-transform: $(RUBY_SRC)/configure.ac $(SYNTAX_TRANSFORM_PATCHES)
ifeq ($(HAVE_SYNTAX_TRANSFORM),1)
ifeq ($(patsubst MINGW%,,$(firstword $(shell uname))),)
	cd $(RUBY_SRC) && for patch in $(SYNTAX_TRANSFORM_PATCHES); do echo "Applying $$patch"; patch -p1 -i $(PWD)/$$patch || exit 1; done
else
	cd $(RUBY_SRC) && for patch in $(SYNTAX_TRANSFORM_PATCHES); do echo "Applying $$patch"; $(GIT) apply $(PWD)/$$patch || exit 1; done
endif
endif
