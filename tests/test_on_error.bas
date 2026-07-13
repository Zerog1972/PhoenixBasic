REM Test ON ERROR GOSUB
ON ERROR GOSUB error_handler
ERROR 42
PRINT "After ERROR (should not print)"
END
error_handler:
PRINT "Error handler called"
RETURN
