package com.gameraccoon.core

import uniffi.client_ffi.Calculator

class SafeCalculator {
  private var calc = Calculator()

  fun add(lhs: Long, rhs: Long): Long {
    return calc.calculate(lhs, rhs)
  }
}
