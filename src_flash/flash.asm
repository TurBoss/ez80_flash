    org 40000h

source:         EQU 50000h
destination:    EQU 0h
length:         EQU 70000h
ackdone:        EQU 70003h

    LD sp, 0BFFFFh

    ; reset ack variable to false
    LD      A, 0h
    LD      (ackdone), A

    ; open flash for write access
    LD      A, 0xB6  ; unlock
    OUT0    (0xF5), A
    LD      A, 49h
    OUT0    (0xF5), A
    LD      A, 0h   ; unprotect all pages
    OUT0    (0xFA), A

    LD      A, 0xB6  ; unlock again
    OUT0    (0xF5), A
    LD      A, 49h
    OUT0    (0xF5), A
    LD      A, 0x5F  ; Ceiling(18Mhz * 5,1us) = 95, or 0x5F
    OUT0    (0xF9), A

    ; mass erase flash
    LD      A, 01h
    OUT0    (0xFF), A
erasewait:
    IN0     A, (0xFF)
    AND     A, 01h
    JR      nz, erasewait

    ; flash
    LD		DE, destination
    LD		HL, source
	LD      BC, (length)
    LDIR

    ; protect flash pages 
    LD      A, 0xB6  ; unlock
    OUT0    (0xF5), A
    LD      A, 49h
    OUT0    (0xF5), A
    LD      A, 0xFF  ; protect all pages
    OUT0    (0xFA), A

    ; set ack variable to done
    LD      A, 1h
    LD      (ackdone), A

end:
    JR end
