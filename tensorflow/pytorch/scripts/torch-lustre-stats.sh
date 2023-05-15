#!/bin/bash

#SBATCH -J torch-lustre-iops     # Job name
#SBATCH -o torch-lustre-iops.o%j # Name of stdout output file
#SBATCH -e torch-lustre-iops.e%j # Name of stderr error file
#SBATCH -p rtx                   # Queue (partition) name
#SBATCH -N 1                     # Total # of nodes (must be 1 for serial)
#SBATCH -n 1                     # Total # of mpi tasks (should be 1 for serial)
#SBATCH -t 1:00:00              # Run time (hh:mm:ss)

ROOT_DIR="${HOME}/maypaper"
REPO="thesis"
WORKSPACE=$ROOT_DIR/$REPO
DATA="/scratch1/07854/dantas/shared"

echo "cp ${DATA}/objects/44g.zip /dev/shm"
cp "${DATA}/objects/44g.zip" /dev/shm
echo "cp ${DATA}/objects/7g.zip /dev/shm"
cp "${DATA}/objects/7g.zip" /dev/shm
echo "cp ${DATA}/objects/6g.zip /dev/shm"
cp "${DATA}/objects/6g.zip" /dev/shm
echo "cp ${DATA}/objects/3g /dev/shm"
cp "${DATA}/objects/3g" /dev/shm

echo "Loading gcc and python modules"
export CC="/opt/apps/gcc/8.3.0/bin/gcc"

echo "module load cuda/10.1/10.1.243 cudnn/7.6/7.6.5 nccl/2.4/2.4.8-1 openmpi/2.1.6"
module load cuda/10.1
module load cudnn/7.6.5
module load nccl/2.5.6

echo "module load remora"
module load remora

cd "${WORKSPACE}/pytorch/scripts"

EPOCHS=3
BATCH_SIZE=256
TARGET_DIR="${ROOT_DIR}/results/lustre-iops/pytorch"

# 100g runs

DATASET="${DATA}/imagenet_processed/100g_tfrecords"
INDEX="${DATA}/idx_files/100g_tfrecords_idx"

export MONARCH_CONFIGS_PATH="${HOME}/maypaper/thesis/configurations/frontera/tf_placement_100g_disk.yaml"

for i in {1..1}; do
  WORKERS=16 
  RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "lenet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/monarch/100g")
  remora ./train.sh -o -n -m lenet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -x -i $INDEX -r $RUN_DIR
  sleep 10
  rm -r "/tmp/100g_tfrecords"
  mv "remora_${SLURM_JOB_ID}" $RUN_DIR

#  #RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "lenet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/vanilla/100g")
#  #remora ./train.sh -o -n -m lenet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -i $INDEX -r $RUN_DIR
#  #sleep 10
#  #mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  WORKERS=8
#  RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "resnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/monarch/100g")
#  remora ./train.sh -o -n -m resnet50 -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -x -i $INDEX -r $RUN_DIR
#  sleep 10
#  rm -r "/tmp/100g_tfrecords"
#  mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  #RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "resnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/vanilla/100g")
#  #remora ./train.sh -o -n -m resnet50 -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -i $INDEX -r $RUN_DIR
#  #sleep 10
#  #mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  WORKERS=16 
#  RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "alexnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/monarch/100g")
#  remora ./train.sh -o -n -m alexnet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -x -i $INDEX -r $RUN_DIR
#  sleep 10
#  rm -r "/tmp/100g_tfrecords"
#  mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  #RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "alexnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/vanilla/100g")
#  #remora ./train.sh -o -n -m alexnet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -i $INDEX -r $RUN_DIR
#  #sleep 10
#  #mv "remora_${SLURM_JOB_ID}" $RUN_DIR
done

# 200g runs

#DATASET="${DATA}/imagenet_processed/200g_2048_tfrecords"
#INDEX="${DATA}/idx_files/200g_2048_tfrecords_idx"
#
#export MONARCH_CONFIGS_PATH="${HOME}/maypaper/thesis/configurations/frontera/tf_placement_200g_disk.yaml"
#
#for i in {1..1}; do
#  WORKERS=16 
#  RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "lenet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/monarch/200g")
#  remora ./train.sh -o -n -m lenet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -x -i $INDEX -r $RUN_DIR
#  sleep 10
#  rm -r "/tmp/200g_tfrecords"
#  mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  #RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "lenet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/vanilla/200g")
#  #remora ./train.sh -o -n -m lenet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -i $INDEX -r $RUN_DIR
#  #sleep 10
#  #mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  WORKERS=16
#  RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "resnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/monarch/200g")
#  remora ./train.sh -o -n -m resnet50 -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -x -i $INDEX -r $RUN_DIR
#  sleep 10
#  rm -r "/tmp/200g_tfrecords"
#  mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  #RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "resnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/vanilla/200g")
#  #remora ./train.sh -o -n -m resnet50 -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -i $INDEX -r $RUN_DIR
#  #sleep 10
#  #mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  WORKERS=16 
#  RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "alexnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/monarch/200g")
#  remora ./train.sh -o -n -m alexnet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -x -i $INDEX -r $RUN_DIR
#  sleep 10
#  rm -r "/tmp/200g_tfrecords"
#  mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#
#  #RUN_DIR=$(${WORKSPACE}/common/get-run-dir.sh "alexnet" $BATCH_SIZE $EPOCHS "${TARGET_DIR}/vanilla/200g")
#  #remora ./train.sh -o -n -m alexnet -b $BATCH_SIZE -e $EPOCHS -w $WORKERS -d $DATASET -i $INDEX -r $RUN_DIR
#  #sleep 10
#  #mv "remora_${SLURM_JOB_ID}" $RUN_DIR
#done
