AliceSoft image format thumbnailer
==================================

Linux (freedesktop) thumbnailer support for AliceSoft image formats.
Note that this will not work in KDE/Dolphin, because they do not use the
freedesktop mechanism for thumbnail creation. See
[AliceThumbnailer](https://github.com/nunuhara/alicethumbnailer) for a KDE
thumbnailer.

Installation
------------

Install the mime types with xdg-mime

    $ xdg-mime install image-ajp.xml
    $ xdg-mime install image-dcf.xml
    $ xdg-mime install image-pms.xml
    $ xdg-mime install image-qnt.xml

Update the mime database

    $ update-mime-database

Install the thumbnailer

    # cp alicetools.thumbnailer /usr/share/thumbnailers/
