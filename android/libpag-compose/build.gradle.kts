plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
    `maven-publish`
}

android {
    namespace = "org.libpag.compose"
    compileSdk = 36

    defaultConfig {
        minSdk = 23

        consumerProguardFiles("proguard-rules.pro")
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlin {
        jvmToolchain(17)
    }

    buildFeatures {
        compose = true
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
        }
    }
}

dependencies {
    implementation("androidx.compose.ui:ui:1.10.2")
    implementation("androidx.compose.foundation:foundation:1.10.2")
    implementation("androidx.compose.runtime:runtime:1.10.2")

    debugApi(project(":libpag"))
    // TODO release
    // releaseApi("com.tencent.tav:libpag:4.5.xx")

}

afterEvaluate {
    publishing {

        publications {

            create<MavenPublication>("release") {
                from(components.findByName("release"))
                groupId = "com.tencent.tav"
                artifactId = "libpag-compose"
                version = "1.0.0"

                pom {
                    name.value("ligpage-compose")
                    description.value("A real-time rendering library for PAG (Portable Animated Graphics) files that renders After Effects animations natively across multiple platforms.")
                    url.value("https://github.com/Tencent/libpag")

                    licenses {
                        license {
                            //协议类型
                            name.value("The MIT License")
                            url.value("https://github.com/Tencent/libpag/blob/main/LICENSE.txt")
                        }
                    }

                    developers {
                        developer {
                            // TODO
                            id.value("xx")
                            name.value("xx")
                            email.value("xx")
                        }
                    }

                    scm {
                        connection.value("scm:git@github.com:Tencent/libpag.git")
                        developerConnection.value("scm:git@github.com:Tencent/libpag.git")
                        url.value("https://github.com/Tencent/libpag")
                    }
                }
            }
        }
        repositories {
            maven {
                // TODO
//                url = uri(mavenUrl)
                credentials {
                    username = "xx"
                    password = "xx"
                }
            }

        }

    }
}