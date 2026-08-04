include("D:/work/T-JSON-V1.0/build_test/.qt/QtDeploySupport-RelWithDebInfo.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/T-JSON-plugins-RelWithDebInfo.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase;qtdeclarative;qtwebengine;qtserialport")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "D:/work/T-JSON-V1.0/build_test/RelWithDebInfo/LSSVideoManager.exe"
    GENERATE_QT_CONF
)
