package com.gameraccoon.easyphotobackup

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.gameraccoon.easyphotobackup.ui.theme.EasyPhotoBackupTheme
import uniffi.client_ffi.ClientStorage

class MainActivityOld : ComponentActivity() {

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)

    val easyPhotoBackupApplication = application as EasyPhotoBackupApplication
    var clientStorage = easyPhotoBackupApplication.getClientStorage()

    setContent {
      Layout(
          onAddDeviceClicked = {},
          onDebugSendFilesClicked = {},
          clientStorage,
      )
    }
  }
}

@Composable
fun DeviceButton(name: String, modifier: Modifier = Modifier) {
  Button(onClick = { println("Button 1 clicked") }) { Text(text = name, modifier = modifier) }
}

@Composable
fun ListOfDevices(clientStorage: ClientStorage?, modifier: Modifier = Modifier) {
  if (clientStorage == null) {
    return
  }

  clientStorage.getPairedServers().forEach { device ->
    Column(modifier = modifier) { DeviceButton(device.getName()) }
  }
}

@Composable
fun AddDeviceButton(onClicked: () -> Unit, modifier: Modifier = Modifier) {
  Button(onClick = onClicked) { Text(text = "Add Device", modifier = modifier) }
}

@Composable
fun DebugSendFilesButton(onClicked: () -> Unit, modifier: Modifier = Modifier) {
  Button(onClick = onClicked) { Text(text = "Send files [Debug]", modifier = modifier) }
}

@Composable
fun Layout(
    onAddDeviceClicked: () -> Unit,
    onDebugSendFilesClicked: () -> Unit,
    clientStorage: ClientStorage?
) {
  EasyPhotoBackupTheme {
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
      Column(Modifier.verticalScroll(rememberScrollState())) {
        ListOfDevices(clientStorage)
        AddDeviceButton(onAddDeviceClicked)
        DebugSendFilesButton(onDebugSendFilesClicked)
      }
    }
  }
}

@Preview(showBackground = true)
@Composable
fun ListOfDevicesPreview() {
  Layout({}, {}, null)
}
