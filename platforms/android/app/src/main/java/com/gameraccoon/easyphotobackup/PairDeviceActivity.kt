package com.gameraccoon.easyphotobackup

import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.widget.EditText
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import uniffi.client_ffi.DiscoveredService
import uniffi.client_ffi.PairingProcessor

class PairDeviceActivity : AppCompatActivity() {
  var discoveredService: DiscoveredService? = null
  var pairingProcessor: PairingProcessor = PairingProcessor()

  @OptIn(DelicateCoroutinesApi::class)
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    enableEdgeToEdge()
    setContentView(R.layout.activity_pair_device)
    ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
      val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
      v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
      insets
    }

    val id = intent.getByteArrayExtra("id")!!
    val ip = intent.getStringExtra("ip")!!
    val port = intent.getIntExtra("port", 0)
    val name = intent.getStringExtra("name")!!
    discoveredService = DiscoveredService.from(id, ip, port, name)

    if (discoveredService != null) {
      var service = discoveredService!!
      var context = this

      GlobalScope.launch {
        val easyPhotoBackupApplication = application as EasyPhotoBackupApplication
        var clientStorage = easyPhotoBackupApplication.getClientStorage()
        if (clientStorage != null) {
          pairingProcessor.pairToServer(service, clientStorage)
          runOnUiThread { showPairingCodeInput() }
        } else {
          Toast.makeText(context, "Client storage is null", Toast.LENGTH_SHORT).show()
        }
      }
    }
  }

  fun showPairingCodeInput() {
    val numericCodeInput = findViewById<EditText>(R.id.numeric_code_input)
    val numericCodeBlock = findViewById<View>(R.id.numeric_code_block)
    numericCodeInput.setOnKeyListener { _, keyCode, event ->
      if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
        validateNumericCode(numericCodeInput)
        true
      } else {
        false
      }
    }
    numericCodeBlock.visibility = View.VISIBLE
    var waitingForCodeText = findViewById<View>(R.id.waiting_for_code_text)
    waitingForCodeText.visibility = View.GONE
  }

  @OptIn(DelicateCoroutinesApi::class)
  fun validateNumericCode(numericCodeInput: EditText) {
    val enteredNumericCode = numericCodeInput.text.toString().toIntOrNull()
    val numericCodeBlock = findViewById<View>(R.id.numeric_code_block)
    if (enteredNumericCode == null) {
      Toast.makeText(
              this, "Entered numeric code is not a number, cannot continue", Toast.LENGTH_LONG)
          .show()
      return
    }
    val expectedNumericCode = pairingProcessor.computeNumericComparisonValue()
    if (expectedNumericCode == null) {
      Toast.makeText(
              this, "Security code for the device is not valid, cannot continue", Toast.LENGTH_LONG)
          .show()
      return
    }
    if (enteredNumericCode == expectedNumericCode.toInt()) {
      numericCodeBlock.visibility = View.GONE
      findViewById<View>(R.id.confirmed_message).visibility = View.VISIBLE

      val easyPhotoBackupApplication = application as EasyPhotoBackupApplication
      var clientStorage = easyPhotoBackupApplication.getClientStorage()
      if (clientStorage != null) {
        pairingProcessor.addAsPaired(clientStorage)
      } else {
        Toast.makeText(this, "Client storage is invalid, cannot continue", Toast.LENGTH_LONG).show()
      }
      val context = this
      GlobalScope.launch {
        delay(3000)
        runOnUiThread {
          val intent = Intent(context, MainActivity::class.java)
          intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
          context.startActivity(intent)
        }
      }
    } else {
      numericCodeInput.visibility = View.GONE
      findViewById<View>(R.id.incorrect_code_message).visibility = View.VISIBLE
    }
  }
}
