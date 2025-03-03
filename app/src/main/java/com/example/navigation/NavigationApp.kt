/*
 * File: NavigationApp.kt
 * Description: Application class for the navigation app, used to initialize Hilt dependency injection.
 * Author: Giuseppe Franco
 * Created: March 2025
 */

package com.example.navigation

import android.app.Application
import dagger.hilt.android.HiltAndroidApp

@HiltAndroidApp
class NavigationApp : Application()
