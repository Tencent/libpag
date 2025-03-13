@echo off

if not exist "%1" (
    mklink /J "%1" "%2"
)