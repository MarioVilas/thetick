pipeline {
    agent {
        dockerfile {}
    }
    stages {
        stage('Build') {
            steps {
                sh 'cd src; TARGET=generic-arm-android     make -sj'
                sh 'cd src; TARGET=generic-arm64-android   make -sj'
                sh 'cd src; TARGET=generic-mips-android    make -sj'
                sh 'cd src; TARGET=generic-mips64-android  make -sj'
                sh 'cd src; TARGET=generic-x86-android     make -sj'
                sh 'cd src; TARGET=generic-x86_64-android  make -sj'
                sh 'cd src; TARGET=generic-arm-linux       make -sj'
                sh 'cd src; TARGET=generic-arm64-linux     make -sj'
                sh 'cd src; TARGET=generic-mips-linux      make -sj'
                sh 'cd src; TARGET=generic-mips64-linux    make -sj'
                sh 'cd src; TARGET=generic-x86-linux       make -sj'
                sh 'cd src; TARGET=generic-x86_64-linux    make -sj'
                sh 'cd src; TARGET=generic-x86-windows     make -sj'
                sh 'cd src; TARGET=generic-x86_64-windows  make -sj'
                sh 'cp tick.py bin/'
                dir('bin') {
                    archiveArtifacts artifacts: '*', onlyIfSuccessful: true
                }
            }
        }
    }
}
