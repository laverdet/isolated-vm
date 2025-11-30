FROM debian
RUN <<DONE
	set -eux
	apt-get update
	apt-get install -y \
		curl \
		git \
		python3 \
	;
	# Otherwise `git config --list --global` fails, which `gclient sync` uses.
	touch ~/.gitconfig
	mkdir -p ~/.config/depot_tools
	echo '{"is-googler":false,"countdown":10,"opt-in":null,"version":3}' > ~/.config/depot_tools/metrics.cfg
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git --depth 1
	PATH=$PATH:/depot_tools update_depot_tools
	# groupadd -g 127 docker
	# useradd -m -u 1001 -g docker runner
	# chown -R runner:docker /depot_tools
	git config --system --add safe.directory '*'
DONE
ENV PATH=$PATH:/depot_tools
COPY ./sync /sync
# USER runner
CMD [ "/sync" ]
