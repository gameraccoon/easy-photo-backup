package com.gameraccoon.easyphotobackup

import android.content.Context
import android.util.AttributeSet

class DiscoveredDeviceView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : androidx.appcompat.widget.LinearLayoutCompat(context, attrs) {
    var service: Int = 0

    init {
        inflate(context, R.layout.discovered_device, this)
    }
}
