PRODUCT_COPY_FILES	+= \
	vendor/nexell/apps/NxIPodAudioPlayer/lib/arm/libnx_rgbrender.so:system/app/NxIPodAudioPlayer/lib/arm/libnx_rgbrender.so	\
	vendor/nexell/apps/NxIPodAudioPlayer/lib/arm/libnxrgbrender.so:system/app/NxIPodAudioPlayer/lib/arm/libnxrgbrender.so	\
	vendor/nexell/apps/NxIPodAudioPlayer/lib/arm/libIPodAudControl.so:system/app/NxIPodAudioPlayer/lib/arm/libIPodAudControl.so	\
	vendor/nexell/apps/NxIPodAudioPlayer/lib/arm/libnxipodservice.so:system/app/NxIPodAudioPlayer/lib/arm/libnxipodservice.so

PRODUCT_PACKAGES += \
	NxIPodAudioPlayer
