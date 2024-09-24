    org 40000h

source:         EQU 50000h
destination:    EQU 00000h
length:         EQU 70000h
ackdone:        EQU 70003h

    LD sp, 0BFFFFh

    ; reset ack variable to false
    LD      A, 0h
    LD      (ackdone), A

    ; open flash for write access
    LD      A, 0x0B6  ; unlock
    OUT    (0x0F5), A
    LD      A, 49h
    OUT    (0x0F5), A
    LD      A, 0h   ; unprotect all pages
    OUT    (0x0FA), A

    LD      A, 0x0B6  ; unlock again
    OUT    (0x0F5), A
    LD      A, 49h
    OUT    (0x0F5), A
    LD      A, 0x05F  ; Ceiling(18Mhz * 5,1us) = 95, or 0x5F
    OUT    (0x0F9), A

    ; mass erase flash
    LD      A, 01h
    OUT    (0x0FF), A
erasewait:
    IN     A, (0x0FF)
    AND     A, 01h
    JR      nz, erasewait

    ; flash
    LD		DE, destination
    LD		HL, source
	LD      BC, (length)
    LDIR

    ; protect flash pages 
    LD      A, 0x0B6  ; unlock
    OUT    (0x0F5), A
    LD      A, 49h
    OUT    (0x0F5), A
    LD      A, 0x0FF  ; protect all pages
    OUT    (0x0FA), A

    ; set ack variable to done
    LD      A, 1h
    LD      (ackdone), A

end:
    JR end
