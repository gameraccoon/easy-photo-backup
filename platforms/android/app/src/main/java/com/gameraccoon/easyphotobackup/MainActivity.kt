package com.gameraccoon.easyphotobackup

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.gameraccoon.core.NSDClient
import com.gameraccoon.easyphotobackup.ui.theme.EasyPhotoBackupTheme
import kotlinx.coroutines.delay
import uniffi.client_ffi.Service

class MainActivity : ComponentActivity() {
  private val nsdClient = NSDClient()

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)

    nsdClient.start_discovery()

    setContent {
      EasyPhotoBackupTheme {
        var status by remember { mutableStateOf("Loading...") }

        // Coroutine that runs once on composition and loops forever
        LaunchedEffect(Unit) {
          while (true) {
            val services: List<Service> = nsdClient.get_services()
            status =
                if (services.isEmpty()) {
                  "No services found"
                } else {
                  services[0].ip
                }
            delay(100)
          }
        }

        Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
          Greeting(status)
        }
      }
    }
  }
}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
  Text(text = "Status: $name", modifier = modifier)
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
  EasyPhotoBackupTheme { Greeting("test") }
}
