package com.gameraccoon.core

import uniffi.client_ffi.BinaryOperator
import uniffi.client_ffi.ComputationException

class SafeMultiply : BinaryOperator {
  override fun perform(lhs: Long, rhs: Long): Long {
    try {
      return Math.multiplyExact(lhs, rhs)
    } catch (e: ArithmeticException) {
      throw ComputationException.Overflow()
    }
  }
}
