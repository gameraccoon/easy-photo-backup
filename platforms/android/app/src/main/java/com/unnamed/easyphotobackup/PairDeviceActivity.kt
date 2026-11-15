package com.unnamed.easyphotobackup

import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import com.unnamed.easyphotobackup.databinding.ActivityPairDeviceBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.time.Duration.Companion.milliseconds

class PairDeviceActivity : AppCompatActivity() {

  private lateinit var binding: ActivityPairDeviceBinding

  private lateinit var clientConfigStorage: ClientConfigStorage
  private var serverInfo: ServerConnectionInfo? = null
  private var pendingServerBinding: PendingServerBinding? = null
  private var pairingInProgress = false

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    enableEdgeToEdge()

    binding = ActivityPairDeviceBinding.inflate(layoutInflater)
    setContentView(binding.root)

    ViewCompat.setOnApplyWindowInsetsListener(binding.root) { v, insets ->
      val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
      v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
      insets
    }

    clientConfigStorage = ClientConfigStorage(filesDir.absolutePath)

    val id = intent.getByteArrayExtra("id")
    val ip = intent.getStringExtra("ip")
    val port = intent.getIntExtra("port", 0)
    if (id == null || ip == null || port == 0) {
      Toast.makeText(this, getString(R.string.pairing_status_invalid_server_info), Toast.LENGTH_LONG).show()
      finish()
      return
    }

    serverInfo = ServerConnectionInfo.create(id, ip, port)
    startPairing()
  }

  private fun startPairing() {
    val info = serverInfo ?: return
    lifecycleScope.launch {
      val pendingBinding = withContext(Dispatchers.IO) {
        PairingHelpers.exchangePairingInformationWithServer(info)
      }

      if (pendingBinding == null) {
        Toast.makeText(
                this@PairDeviceActivity,
          getString(R.string.pairing_status_could_not_pair), Toast.LENGTH_LONG)
            .show()
        finish()
        return@launch
      }

      pendingServerBinding = pendingBinding
      showPairingCodeInput()
    }
  }

  private fun showPairingCodeInput() {
    binding.numericCodeInput.setOnKeyListener { _, keyCode, event ->
      if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
        validateNumericCode(binding.numericCodeInput)
        true
      } else {
        false
      }
    }
    binding.numericCodeBlock.visibility = View.VISIBLE
    val waitingForCodeText = binding.waitingForCodeText
    waitingForCodeText.visibility = View.GONE
  }

  private fun validateNumericCode(numericCodeInput: EditText) {
    if (pairingInProgress) {
      return
    }

    val enteredNumericCodeText = numericCodeInput.text.toString().trim()
    if (enteredNumericCodeText.toIntOrNull() == null) {
      Toast.makeText(
              this, getString(R.string.security_code_nan), Toast.LENGTH_LONG)
          .show()
      return
    }

    if (pendingServerBinding == null || serverInfo == null) {
      Toast.makeText(
              this, getString(R.string.security_code_not_valid), Toast.LENGTH_LONG)
          .show()
      return
    }

    hideKeyboard()
    numericCodeInput.visibility = View.GONE
    binding.numericCodeBlock.visibility = View.GONE

    if (enteredNumericCodeText != pendingServerBinding!!.generateShortAuthentificationString().trim()) {
      binding.incorrectCodeMessage.visibility = View.VISIBLE
      return
    }

    pairingInProgress = true

    binding.serverNameInput.setOnKeyListener { _, keyCode, event ->
      if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
        onServerNameChosen(binding.serverNameInput)
        true
      } else {
        false
      }
    }

    binding.numericCodeBlock.visibility = View.GONE
    binding.serverNameBlock.visibility = View.VISIBLE
  }

  private fun hideKeyboard() {
    this.currentFocus?.let { view ->
      val imm = getSystemService(INPUT_METHOD_SERVICE) as? InputMethodManager
      imm?.hideSoftInputFromWindow(view.windowToken, 0)
      }
  }

  fun onServerNameChosen(serverNameInput: EditText)
  {
    val serverName = serverNameInput.text.toString().trim()
    // ToDo: sanitize the name

    lifecycleScope.launch {
      if (serverInfo == null || pendingServerBinding == null)
      {
        pairingInProgress = false
        return@launch
      }

      val error = withContext(Dispatchers.IO) {
        PairingHelpers.approveServer(clientConfigStorage, serverInfo!!, pendingServerBinding!!, serverName)
      }

      if (error != null) {
        Toast.makeText(this@PairDeviceActivity, error, Toast.LENGTH_LONG).show()
        pairingInProgress = false
        return@launch
      }

      hideKeyboard()
      binding.serverNameBlock.visibility = View.GONE
      binding.confirmedMessage.visibility = View.VISIBLE

      delay(3000.milliseconds)

      val intent = Intent(this@PairDeviceActivity, MainActivity::class.java)
      intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
      startActivity(intent)
    }
  }

  override fun onDestroy() {
    pendingServerBinding?.close()
    pendingServerBinding = null
    serverInfo?.close()
    serverInfo = null
    clientConfigStorage.close()
    super.onDestroy()
  }
}
