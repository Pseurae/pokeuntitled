
TOOLDIRS := $(filter-out agbcc poryscript binutils Makefile, $(wildcard *))

.PHONY: all $(TOOLDIRS) poryscript

all: $(TOOLDIRS) poryscript

$(TOOLDIRS):
	@$(MAKE) -C $@

poryscript:
	@cd poryscript && go build
	@cd ../..

clean:
	@$(foreach tooldir,$(TOOLDIRS),$(MAKE) clean -C $(tooldir);)
	rm -f poryscript/poryscript