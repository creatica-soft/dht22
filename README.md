# DHT11/22
Arduino sketch env_dht22.ino for DHT11/22 sensor (does not need any external library)

Raspberry Pi dht.c file uses /dev/gpiomem with mmap. To run it under a non-root account, change group and mode of /dev/gpiomem file. For example,

```
chgrp dialout /dev/gpiomem
chmod g+rw /dev/gpiomem
```

To preserve your changes across reboots, create /etc/udev/rules.d/00-gpiomem.rules file with this line:

```
KERNEL=="gpiomem*",GROUP="dialout",MODE="0660"
```

Finally, add a user to group dialout in /etc/group, log off and log on.
