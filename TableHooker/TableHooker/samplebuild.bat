@echo off
msbuild TableHooker.vcxproj /t:Rebuild /p:Configuration="Debug";Platform=x64
IF %ERRORLEVEL% NEQ 0 GOTO ERROR

:OK
echo ################################
echo ################################
echo         Build Success!!
echo ################################
echo ################################
goto end

:ERROR
echo ################################
echo ################################
echo         Build Error!!
echo ################################
echo ################################
goto end

:end