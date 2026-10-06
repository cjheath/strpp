idf_component_register(
		SRCS "wifi_scanner.cpp"
		INCLUDE_DIRS "."
		REQUIRES strpp
		PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash
	)
