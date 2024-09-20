#importonce

/*

    Labels for common BASIC routines

*/
.namespace basic {
    .label RESLST               = $a09e
    .label ERROR                = $a437
    .label CUSTERROR            = $a445
    .label NEWSTT               = $a7ae
    .label EXECOLD              = $a7ed
    .label CHAROUT              = $ab47
    .label STROUT               = $ab1e
    .label FRMNUM               = $ad8a
    .label FRMEVL               = $ad9e
    .label EVAL                 = $ae83
    .label FUNCTOLD             = $ae8d
    .label PARCHK               = $aef1
    .label CHKCOM               = $aefd
    .label FACINX               = $b1aa
    .label ILLEGAL_QUANTITY     = $b248
    .label FRESTR               = $b6a3
    .label GETBYTC              = $b79b
    .label GETADR               = $b7f7
    .label OVERR                = $b97e
    .label FINLOG               = $bd7e

    .label ERROR_TOO_MANY_FILES         = $01
    .label ERROR_FILE_OPEN              = $02
    .label ERROR_FILE_NOT_OPEN          = $03
    .label ERROR_FILE_NOT_FOUND         = $04
    .label ERROR_DEVICE_NOT_PRESENT     = $05
    .label ERROR_NOT_INPUT_FILE         = $06
    .label ERROR_NOT_OUTPUT_FILE        = $07
    .label ERROR_MISSING_FILENAME       = $08
    .label ERROR_ILLEGAL_DEVICE_NUM     = $09
    .label ERROR_NEXT_WITHOUT_FOR       = $0a
    .label ERROR_SYNTAX                 = $0b
    .label ERROR_RETURN_WITHOUT_GOSUB   = $0c
    .label ERROR_OUT_OF_DATA            = $0d
    .label ERROR_ILLEGAL_QUANTITY       = $0e
    .label ERROR_OVERFLOW               = $0f
    .label ERROR_OUT_OF_MEMORY          = $10
    .label ERROR_UNDEFD_STATEMENT       = $11
    .label ERROR_BAD_SUBSCRIPT          = $12
    .label ERROR_REDIMD_ARRAY           = $13
    .label ERROR_DIVISION_BY_ZERO       = $14
    .label ERROR_ILLEGAL_DIRECT         = $15
    .label ERROR_TYPE_MISMATCH          = $16
    .label ERROR_STRING_TOO_LONG        = $17
    .label ERROR_FILE_DATA              = $18
    .label ERROR_FORMULA_TOO_COMPLEX    = $19
    .label ERROR_CANT_CONTINUE          = $1a
    .label ERROR_UNDEFD_FUNCTION        = $1b
    .label ERROR_VERIFY                 = $1c
    .label ERROR_LOAD                   = $1d
}
