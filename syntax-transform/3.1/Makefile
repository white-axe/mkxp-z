SYNTAX_TRANSFORM_PATCHES := \
	$(SYNTAX_TRANSFORM_DIR)/0001-prelude.patch \
	$(SYNTAX_TRANSFORM_DIR)/0002-space-before-argument-parentheses.patch \
	$(SYNTAX_TRANSFORM_DIR)/0003-singleton-class-superclass.patch \
	$(SYNTAX_TRANSFORM_DIR)/0004-hash-args.patch \
	$(SYNTAX_TRANSFORM_DIR)/0005-block-variable-shadowing.patch \
	$(SYNTAX_TRANSFORM_DIR)/0006-then-do-colon.patch \
	$(SYNTAX_TRANSFORM_DIR)/0007-character-literal.patch \
	$(SYNTAX_TRANSFORM_DIR)/0008-multiple-values-for-a-block-parameter.patch \
	$(SYNTAX_TRANSFORM_DIR)/0009-else-without-rescue.patch \

HAVE_SYNTAX_TRANSFORM ?= 1

syntax-transform: $(RUBY_SRC)/configure.ac $(SYNTAX_TRANSFORM_PATCHES)
ifeq ($(HAVE_SYNTAX_TRANSFORM),1)
	cd $(RUBY_SRC) && for patch in $(SYNTAX_TRANSFORM_PATCHES); do $(GIT) apply $(PWD)/$$patch; done
endif
