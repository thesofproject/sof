#!/bin/bash
if test -f "found.txt"; then
	rm found.txt
fi
if test -f "found2.txt"; then
	rm found2.txt
fi
if test -f "foundTests.txt"; then
	rm found_tests.txt
fi
if test -f "Tests.txt"; then
	rm Tests.txt
fi
if test -f "not_ound_tests.txt"; then
	rm not_found_tests.txt
fi
if test -f "foundSec.txt"; then
	rm foundSec.txt
fi
if test -f "foundTestsSec.txt"; then
	rm foundTestsSec.txt
fi
if test -f "found_gprof.txt"; then
	rm found_gprof.txt
fi
if test -f "found_gprof_filtered.txt"; then
	rm found_gprof_filtered.txt
fi
if test -f "found_tests_gprof_filtered.txt"; then
	rm found_tests_gprof_filtered.txt
fi
if test -f "not_found_tests.txt"; then
	rm not_found_tests.txt
fi

#get all funcitons in sof project which are used
xt-objdump -x sof | awk '$4 == ".text" {print $6}' | sort -u  \
| grep -v "__\|\.text" > sof.txt
cd test

#find all tests - executables
find -L -type f -perm -111 | while read executablePath;
do
    fileName=$(basename $executablePath);

    #read test names
    xt-objdump -x $executablePath | awk '$4 == ".text" {print $6}'\
      | sort -u | grep -v "__\|\.text" | grep "^test" | while read testNames;
    do
        #save found test names
        echo $testNames >> "../Tests.txt";
	found=0;
	while read functions;
	do
	#compare test name contains function name then found
	#we believe that programmer test that function, it is more reliable
	#than comparing with all functions in the sof
	    if [[ $testNames =~ $functions ]];
	    then
	        echo $functions >> "../found.txt";
		echo $testNames >> "../found_tests.txt";

		#skip other checks
		found=1;
	    fi;
	    done < ../sof.txt
	    while read macros;
	    do
	    #check if the function name is not on the forbbiden list
	    if [[ $testNames =~ $macros ]];
	    then
	        echo $fileName >> "../found_macros.txt";

		#skip other checks
		found=1;
	    fi
	    done < ../sof-test-coverage-macro-functions.txt

	    #if function was not found after above searches
	    if [[ $found == 0 ]] && [[ $testNames =~ '_'$fileName ]];
	    then
		#then find file name cause it may contain searched function
		while read functions;
		do
		    if [[ $functions =~ $fileName ]];
		    then
			echo $fileName >> "../found.txt";
			echo $testNames >> "../found_tests.txt";
			found=1;
		    fi
			done
		fi
		if [[ $found == 0 ]];
		then
		    directPath=$(dirname $executablePath);

		    #otherwise run profiler, it's necessary step to run gprof
		    xt-run --profile=${directPath}/gmon${fileName}.out \
		      $executablePath > null

		    #run gprof get function number, and find tested function
		    FUNCTION_NUMBER="$(xt-gprof -P $executablePath \
		    $directPath/gmon$fileName.out | grep $testNames | awk -v \
		      var="$testNames" '$4 == var {print $5}  NR==1{exit}')"
	xt-gprof -q -b $executablePath ${directPath}/gmon${fileName}.out \
	  | awk -v find_gprof_index="$FUNCTION_NUMBER" -v var="$testNames" \
	  '$1 == find_gprof_index {getline; while
           ($0 != "-----------------------------------------------")
	   {if ($4 ~ /assert/){break;}; if(!($4 ~ /__/))
	   {print $4; print var} getline}}' >> ../found_gprof.txt
		    while read check;
		    do
		        read test_name_gprof;

			#compare functions from gprof with the searched ones
			while read functions;
			do
			    if [[ $check == $functions ]];
			    then
				echo $functions >> "../found.txt";
				echo $test_name_gprof >> "../found_tests.txt";
				found=1;
				break;
			    fi;
			done < ../sof.txt
		    done < ../found_gprof.txt
		fi
		if [[ $found == 0 ]];
		then
			echo $testNames >> "../not_found_tests.txt";
		fi
	  done
done
cd ..
sort -u found.txt > found_sorted.txt
sort -u found_macros.txt > found_macros_sorted.txt
found_num=$(sort -u found.txt | wc -l)
found_exception_num=$(sort -u found_macros.txt | wc -l)
function_total_num=$(sort -u sof.txt | wc -l)
not_found_num=$(sort -u not_found_tests.txt | wc -l)
result=`echo "scale=2; $found_num*100/$function_total_num" | bc`
echo "Found $found_num matching test(s) of $function_total_num which
 consists $result%. Found $found_exception_num test(s) which does not belong
 to function coverage (may check struct, macro or sth else)
 $found_exception_num and $not_found_num test(s) which does not
 seem to cover any function in library"
if test -f found_gprof.txt; then
	rm found_gprof.txt
fi
