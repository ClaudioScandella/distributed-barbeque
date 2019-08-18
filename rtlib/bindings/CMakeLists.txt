if (CONFIG_BBQUE_BINDINGS_PYTHON)
  add_subdirectory (python)
else()
endif (CONFIG_BBQUE_BINDINGS_PYTHON)

if (CONFIG_BBQUE_BINDINGS_JAVA)
  add_subdirectory (java)
else()
endif (CONFIG_BBQUE_BINDINGS_JAVA)

