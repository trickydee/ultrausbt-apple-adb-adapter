/*
    SideWinder 3D Pro ADB packet:

    0         1          2         3          4         5          6
    bbbb xxxx xxxx xxyy  yyyy yyyy hhhh 000r  rrrr rrrr bbbb 00tt  tttt tttt
    b                                                                         base bottom left 1=off 0=on
     b                                                                        base bottom right
      b                                                                       base top right
       b                                                                      base top left
         xxxx xxxx xx                                                         x axis 0=left..3FF=right
                     yy  yyyy yyyy                                            y axis 0=up..3FF=down
                                   hhhh                                       hat 0=off, 1=up, 2=upleft, ..., 8=upright
                                        000
                                           r  rrrr rrrr                       rudder 0=counter clockwise ... 1FF=clockwise
                                                        b                     side bottom trigger
                                                         b                    side top trigger
                                                          b                   top trigger
                                                           b                  main trigger
                                                             00
                                                               tt  tttt tttt  throttle 000=up...3FF=down
    */
