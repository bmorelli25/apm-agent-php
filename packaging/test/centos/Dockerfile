FROM centos:centos7
ARG PHP_VERSION=7.2
ENV PHP_VERSION ${PHP_VERSION}
## Package versioning for the PHP does not use .
RUN export PHP_VERSION_TRANSFORMED=$(echo "${PHP_VERSION}" | sed 's#\.##g') \
    && yum install -y epel-release yum-utils \
    && rpm -Uvh http://rpms.remirepo.net/enterprise/remi-release-7.rpm \
    && yum update -y \
    && yum-config-manager --enable remi-php${PHP_VERSION_TRANSFORMED} \
    && yum install -y php php-mbstring php-mysql php-xml

RUN curl -sS https://getcomposer.org/installer | php && mv composer.phar /usr/bin/composer

COPY entrypoint.sh /bin
WORKDIR /src

ENTRYPOINT ["/bin/entrypoint.sh"]