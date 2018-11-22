#!/bin/sh

modname=pages_view.ko
# in case we are working in VM shared folder
cp $modname /tmp/$modname
insmod /tmp/$modname
