package com.gameraccoon.core

import uniffi.client_ffi.DiscoveredService
import uniffi.client_ffi.NetworkServiceDiscoveryClient

class NSDClient {
  private var client = NetworkServiceDiscoveryClient()

  fun startDiscovery(discoveryPeriodMs: ULong) {
    client.start(discoveryPeriodMs)
  }

  fun stopDiscovery(shouldWaitForThreadJoin: Boolean) {
    client.stop(shouldWaitForThreadJoin)
  }

  fun getServices(): List<DiscoveredService> {
    return client.getServices()
  }
}
