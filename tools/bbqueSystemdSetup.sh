#!/bin/bash

echo "Systemd configuration files patch: cpuset, cpu, cpuacct, memory"
echo "controllers hierarchy unification"

if [ ! -f /etc/systemd/system.conf ]; then
        echo "Systemd does not appear to be up and running"
        exit
else
        SDV=$(systemd --version | head -n1 | awk '{print $2}')
        echo "Systemd version = $SDV"

        echo "[WARNING] This is an experimental patch. Apply it only if you "
        echo -e "cannot start barbeque due to mount errors.\n"
        read -p "Continue anyway? [y/N] " ANS

fi

cat /etc/systemd/system.conf | sed -r 's/.*JoinControllers.*/JoinControllers=cpu,cpuset,cpuacct,memory,net_cls net_prio/g' > systemdconf.bbque.tmp

diff /etc/systemd/system.conf systemdconf.bbque.tmp > systemdconf.bbque.diff

if [ "$(cat systemdconf.bbque.diff)" == "" ]; then
        echo "No changes to do"
        exit
fi

echo -e "\nIn order for the BarbequeRTRM and systemd to co-exist peacefully,"
echo  "We propose the following changes in the systemd configuration file"
echo -e "[/etc/systemd/system.conf ] :\n"
cat systemdconf.bbque.diff
echo -e "\n"

rm systemdconf.bbque.diff

read -p "Apply? [y/N] " ANS

if [ "$ANS" == "y" ] || [ "$ANS" == "Y" ]; then
        echo "Changes will be applied."
        sudo mv systemdconf.bbque.tmp /etc/systemd/system.conf
else
        rm systemdconf.bbque.tmp
        exit
fi

read -p "System needs to be rebooted. Do it now? [y/N] " ANS

if [ "$ANS" != "y" ] && [ "$ANS" != "Y" ]; then
	exit
fi

sudo reboot
