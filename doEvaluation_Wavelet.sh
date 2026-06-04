
dir="ICC-G"
subdir="N"
endtime=30

# CC_mode: 0-NewReno 1-PDCC 2-Copa 3-BBR 4-ADC 5-Cubic 6-NewReno(dup) 7-ICC-G
CC_mode=7

list="4"


for loop in $list #1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 #4 8 12 16 20 24 28 32 #36 40 44 48 #20 30 40 # 12 14 16 18 20 22 24 26 #2 4 6 8 10 20 30 40 #2 4 6 8 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160
do
postfix=$loop
./waf --run "periodcCCEvaDumbbellforWavelet --runtime=$endtime --flowNum=$loop --delayRB=20 --BW=48 --Bd=10 --Rc=30 --lamuda=1 --postfix=$postfix --rand_loss=false --rand_BW=true --rand_delay=false --rand_interval=20 --queuesize=320 --cycle=0.5 --CC_mode=$CC_mode"  > ./Testdata/Wavelet/"$dir"/output.txt 2>&1 \
&& \
# cd ./Testdata/Evaluation
# echo seperate
# bash doseprate.sh $postfix $postfix $dir\
# && \
# echo cat throughput
# bash doAllThr.sh $postfix $postfix $dir 0.5\
# && \
# echo cat Ideal
# bash docatIdeal.sh $postfix 5 $dir\
# && \
# echo paint throughput
# bash plotThrAnalyse.sh $postfix $postfix $dir\

# echo paint other
# bash plotCwndAnalyse.sh $postfix $postfix $dir $endtime


#rm -rf "$dir"/"$subdir"

# mkdir ./Testdata/Wavelet/$dir/$subdir

# #mv "$dir"/flows$loop "$dir"/"$subdir"
# mv ./Testdata/Wavelet/"$dir"/output"$postfix".txt ./Testdata/Wavelet/"$dir"/"$subdir"
# mv ./Testdata/Wavelet/"$dir"/PDCC.q ./Testdata/Wavelet/"$dir"/"$subdir"/"$dir"_"$postfix".q
# mv ./Testdata/Wavelet/"$dir"/PDCC.tr ./Testdata/Wavelet/"$dir"/"$subdir"/"$dir"_"$postfix".tr

echo "Done!"

done
