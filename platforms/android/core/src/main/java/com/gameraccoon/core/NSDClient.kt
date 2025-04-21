package com.gameraccoon.core

import uniffi.client_ffi.NetworkServiceDiscoveryClient
import uniffi.client_ffi.Service

class NSDClient {
  private var client = NetworkServiceDiscoveryClient()

  fun start_discovery() {
    client.start()
  }

  fun stop_discovery(shouldWaitForThreadJoin: Boolean) {
    client.stop(shouldWaitForThreadJoin)
  }

  fun get_services(): List<Service> {
    return client.getServices()
  }
}
