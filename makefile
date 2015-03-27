default: all

.DEFAULT:
	cd posa_linux && $(MAKE) $@
	cd rtmpclient && $(MAKE) $@
	cd rtmpserver && $(MAKE) $@
	cd demo && $(MAKE) $@
	@echo ">>>>>>> Congratulations, Compile finished! @><@ <<<<<<<"

clean:
	cd posa_linux && $(MAKE) $@
	cd rtmpclient && $(MAKE) $@
	cd rtmpserver && $(MAKE) $@
	cd demo && $(MAKE) $@
	rm -rf ./bin/*
