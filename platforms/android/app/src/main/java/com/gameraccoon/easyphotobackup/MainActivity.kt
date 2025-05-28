package com.gameraccoon.easyphotobackup

import android.content.Intent
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

class MainActivity : ComponentActivity() {
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)

    setContent {
      Layout(
          onAddDeviceClicked = {
            val context = this
            val intent = Intent(context, DiscoverDevicesActivity::class.java)
            context.startActivity(intent)
          },
          onDebugSendFilesClicked = {})
    }
  }
}

@Composable
fun DeviceButton(name: String, modifier: Modifier = Modifier) {
  Button(onClick = { println("Button 1 clicked") }) { Text(text = name, modifier = modifier) }
}

@Composable
fun ListOfDevices(modifier: Modifier = Modifier) {
  for (i in 1..10) {
    Column(modifier = modifier) { DeviceButton("Test device $i") }
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
fun Layout(onAddDeviceClicked: () -> Unit, onDebugSendFilesClicked: () -> Unit) {
  EasyPhotoBackupTheme {
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
      Column(Modifier.verticalScroll(rememberScrollState())) {
        ListOfDevices()
        AddDeviceButton(onAddDeviceClicked)
        DebugSendFilesButton(onDebugSendFilesClicked)
      }
    }
  }
}

@Preview(showBackground = true)
@Composable
fun ListOfDevicesPreview() {
  Layout({}, {})
}
