package com.edgeclear

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat

/**
 * MainActivity: Entry point for EdgeClear app
 *
 * Responsibilities:
 * - Request permissions (RECORD_AUDIO, CAMERA)
 * - Navigate between Monitor and Record screens
 * - Handle app lifecycle (backgrounding, foregrounding)
 *
 * See data-model.md and contracts/ui-state.md for state management.
 */
class MainActivity : ComponentActivity() {

    private var permissionsGranted by mutableStateOf(false)
    private var audioPermissionGranted by mutableStateOf(false)
    private var cameraPermissionGranted by mutableStateOf(false)

    // Permission launcher
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        audioPermissionGranted = permissions[Manifest.permission.RECORD_AUDIO] == true
        cameraPermissionGranted = permissions[Manifest.permission.CAMERA] == true

        permissionsGranted = audioPermissionGranted

        if (!audioPermissionGranted) {
            Toast.makeText(
                this,
                "Audio recording permission is required for EdgeClear",
                Toast.LENGTH_LONG
            ).show()
        }

        if (!cameraPermissionGranted) {
            Toast.makeText(
                this,
                "Camera permission denied - AV-VAD will not be available",
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Check permissions
        checkPermissions()

        setContent {
            EdgeClearApp(
                permissionsGranted = permissionsGranted,
                cameraAvailable = cameraPermissionGranted,
                onRequestPermissions = { requestPermissions() }
            )
        }
    }

    private fun checkPermissions() {
        audioPermissionGranted = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.RECORD_AUDIO
        ) == PackageManager.PERMISSION_GRANTED

        cameraPermissionGranted = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.CAMERA
        ) == PackageManager.PERMISSION_GRANTED

        permissionsGranted = audioPermissionGranted

        if (!permissionsGranted) {
            requestPermissions()
        }
    }

    private fun requestPermissions() {
        permissionLauncher.launch(
            arrayOf(
                Manifest.permission.RECORD_AUDIO,
                Manifest.permission.CAMERA
            )
        )
    }
}

/**
 * Main app composable
 */
@Composable
fun EdgeClearApp(
    permissionsGranted: Boolean,
    cameraAvailable: Boolean,
    onRequestPermissions: () -> Unit
) {
    MaterialTheme {
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background
        ) {
            if (!permissionsGranted) {
                PermissionScreen(onRequestPermissions = onRequestPermissions)
            } else {
                // Monitor screen (M1 complete!)
                com.edgeclear.ui.MonitorScreen()
            }
        }
    }
}

@Composable
fun PermissionScreen(onRequestPermissions: () -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text(
            text = "EdgeClear Audio Enhancement",
            style = MaterialTheme.typography.headlineMedium
        )
        Spacer(modifier = Modifier.height(16.dp))
        Text(
            text = "Audio recording permission is required",
            style = MaterialTheme.typography.bodyLarge
        )
        Spacer(modifier = Modifier.height(32.dp))
        Button(onClick = onRequestPermissions) {
            Text("Grant Permissions")
        }
    }
}

@Composable
fun PlaceholderScreen() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text(
            text = "EdgeClear",
            style = MaterialTheme.typography.headlineLarge
        )
        Spacer(modifier = Modifier.height(16.dp))
        Text(
            text = "Monitor and Record screens coming in Phase 3 (M1)",
            style = MaterialTheme.typography.bodyLarge
        )
    }
}
