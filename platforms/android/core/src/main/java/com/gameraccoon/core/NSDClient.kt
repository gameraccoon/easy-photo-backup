package com.gameraccoon.core

import uniffi.client_ffi.Calculator

class NSDClient {
  private var calc = Calculator()

  fun start_discovery() {
    calc.start()
  }

  fun stop_discovery(shouldWaitForThreadJoin: Boolean) {
    calc.stop(shouldWaitForThreadJoin)
  }
}
