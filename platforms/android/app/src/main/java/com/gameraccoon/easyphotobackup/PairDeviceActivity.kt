package com.gameraccoon.easyphotobackup

import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.widget.EditText
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import uniffi.client_ffi.ClientStorage
import uniffi.client_ffi.DiscoveredService
import uniffi.client_ffi.PairingProcessor

class PairDeviceActivity : AppCompatActivity() {
  var discoveredService: DiscoveredService? = null
  var pairingProcessor: PairingProcessor = PairingProcessor()
  // ToDo: this should be stored in a more permanent place not to conflict with sending files
  var clientStorage: ClientStorage = ClientStorage()

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

      // start a coroutine to listen for new devices once in 100ms
      GlobalScope.launch {
        // ToDo: this should be set globally
        clientStorage.setDeviceName("Android device")
        pairingProcessor.pairToServer(service, clientStorage)
        // run in main thread
        runOnUiThread { showPairingCodeInput() }
      }
    }
  }

  fun showPairingCodeInput() {
    val numericCodeInput = findViewById<EditText>(R.id.numeric_code_input)
    numericCodeInput.setOnKeyListener { _, keyCode, event ->
      if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
        validateNumericCode(numericCodeInput)
        true
      } else {
        false
      }
    }
    numericCodeInput.visibility = View.VISIBLE
    var waitingForCodeText = findViewById<View>(R.id.waiting_for_code_text)
    waitingForCodeText.visibility = View.GONE
  }

  fun validateNumericCode(numericCodeInput: EditText) {
    val enteredNumericCode = numericCodeInput.text.toString().toIntOrNull()
    if (enteredNumericCode == null) {
      println("Entered numeric code is not a number")
      return
    }
    val expectedNumericCode = pairingProcessor.computeNumericComparisonValue()
    if (expectedNumericCode == null) {
      println("Expected code is not valid")
      return
    }
    if (enteredNumericCode == expectedNumericCode.toInt()) {
      println("Numeric code is correct")
      numericCodeInput.visibility = View.GONE
    }
    else {
      println("Numeric code is incorrect")
      numericCodeInput.visibility = View.GONE
    }
  }
}
