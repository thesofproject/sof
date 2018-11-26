install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(PEM_KEY_PREFIX); 
	$(INSTALL_DATA) otc_private_key.pem $(PEM_KEY_PREFIX)
	$(INSTALL_DATA) otc_public_key.pem $(PEM_KEY_PREFIX)

EXTRA_DIST = \
	otc_private_key.pem \
	otc_public_key.pem