package com.gameraccoon.core

import uniffi.client_ffi.NetworkServiceDiscoveryClient
import uniffi.client_ffi.Service

class NSDClient {
  private var client = NetworkServiceDiscoveryClient()

  fun startDiscovery() {
    client.start()
  }

  fun stopDiscovery(shouldWaitForThreadJoin: Boolean) {
    client.stop(shouldWaitForThreadJoin)
  }

  fun getServices(): List<Service> {
    return client.getServices()
  }
}
