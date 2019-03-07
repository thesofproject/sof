#!/bin/bash

PASSED_INFO="Test Passed"
FAILED_INFO="Test Failed"

function __case_passed
{
    __OCCUPY_LINE_DELETE_ME__ #case passed post response
}

function __case_failed
{
    __OCCUPY_LINE_DELETE_ME__ #case failed post response
}

function __execute
{
    arecord -l | grep "device" > /dev/null
}
