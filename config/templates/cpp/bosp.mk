
ifdef CONFIG_CONTRIB_MYAPP

# Targets provided by this project
.PHONY: myapp clean_myapp

# Add this to the "contrib_testing" target
testing: myapp
clean_testing: clean_myapp

MODULE_CONTRIB_USER_MYAPP=contrib/user/MyApp

myapp: external
	@echo
	@echo "==== Building MyApp ($(BUILD_TYPE)) ===="
	@[ -d $(MODULE_CONTRIB_USER_MYAPP)/build/$(BUILD_TYPE) ] || \
		mkdir -p $(MODULE_CONTRIB_USER_MYAPP)/build/$(BUILD_TYPE) || \
		exit 1
	@cd $(MODULE_CONTRIB_USER_MYAPP)/build/$(BUILD_TYPE) && \
		CXX=$(CXX) CFLAGS="--sysroot=$(PLATFORM_SYSROOT)" \
		cmake $(CMAKE_COMMON_OPTIONS) || \
		exit 1
	@cd $(MODULE_CONTRIB_USER_MYAPP)/build/$(BUILD_TYPE) && \
		make -j$(CPUS) install || \
		exit 1

clean_myapp:
	@echo
	@echo "==== Clean-up MyApp Application ===="
	@[ ! -f $(BUILD_DIR)/usr/bin/myapp ] || \
		rm -f $(BUILD_DIR)/etc/bbque/recipes/MyApp*; \
		rm -f $(BUILD_DIR)/usr/bin/myapp*
	@rm -rf $(MODULE_CONTRIB_USER_MYAPP)/build
	@echo

else # CONFIG_CONTRIB_MYAPP

myapp:
	$(warning contib module MyApp disabled by BOSP configuration)
	$(error BOSP compilation failed)

endif # CONFIG_CONTRIB_MYAPP

