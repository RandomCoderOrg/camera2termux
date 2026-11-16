plugins { id("com.android.application") }

android {
    namespace = "org.randomcoder.udroid.camera"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "org.randomcoder.camera2termux"
        minSdk = 31
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86", "x86_64") }
        externalNativeBuild { cmake { cppFlags += "" } }
    }
    signingConfigs {
        create("termuxGithub") {
            storeFile = file("${rootProject.projectDir}/termux-testkey.jks")
            storePassword = "xrj45yWGLbsO7W0v"
            keyAlias = "alias"
            keyPassword = "xrj45yWGLbsO7W0v"
        }
    }
    buildTypes {
        getByName("debug") { signingConfig = signingConfigs.getByName("termuxGithub") }
        getByName("release") {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("termuxGithub")
        }
    }
    externalNativeBuild { cmake { path = file("src/main/cpp/CMakeLists.txt") } }
}
