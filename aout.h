
#pragma once

void aoutdev_init();
void *aoutdev_new();
void aoutdev_close(void *dev);
void aoutdev_write(void *_dev, void *buf, int len);

