find_package(Java REQUIRED)
if (JAVA_FOUND)
	include(UseJava)
else ()
	message (FATAL_ERROR "Unable to find JAVA.")
endif()
project(RTLibJavaSdk Java)
set(SRC
    bbque/rtlib/RTLib.java
    bbque/rtlib/enumeration/Constants.java
    bbque/rtlib/enumeration/RTLibConstraintOperation.java
    bbque/rtlib/enumeration/RTLibConstraintType.java
    bbque/rtlib/enumeration/RTLibExitCode.java
    bbque/rtlib/enumeration/RTLibResourceType.java
    bbque/rtlib/exception/RTLibException.java
    bbque/rtlib/exception/RTLibRegistrationException.java
    bbque/rtlib/model/BbqueEXC.java
    bbque/rtlib/model/RTLibAPIVersion.java
    bbque/rtlib/model/RTLibConfig.java
    bbque/rtlib/model/RTLibConstraint.java
    bbque/rtlib/model/RTLibServices.java
    bbque/rtlib/model/RTLibSystemResources.java
    bbque/rtlib/model/RTLibWorkingModeParams.java
)
add_jar(RTLibJavaSdk ${SRC})
install_jar(RTLibJavaSdk ${BBQUE_PATH_BINDINGS}/java)
