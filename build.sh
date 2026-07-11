#!/bin/sh

set -eu

PORTNAME="pfSense-pkg-ont-monitor"
ABI="FreeBSD:15:amd64"
PREFIX="/usr/local"
ROOT=$(cd -- "$(dirname -- "$0")" && pwd)
FILES="${ROOT}/files"
BUILD="${ROOT}/build"
STAGE="${BUILD}/stage"
OUTPUT="${BUILD}/pkg"
PORTVERSION="${PORTVERSION:?Set PORTVERSION from the release tag without its v prefix}"

clean() {
	rm -rf "${BUILD}"
}

json_escape() {
	awk '{ gsub(/\\/, "\\\\"); gsub(/\t/, "\\t"); gsub(/"/, "\\\""); printf "%s\\n", $0 }'
}

stage() {
	rm -rf "${STAGE}"
	mkdir -p \
		"${STAGE}${PREFIX}/pkg" \
		"${STAGE}${PREFIX}/www/widgets/widgets" \
		"${STAGE}${PREFIX}/www" \
		"${STAGE}${PREFIX}/share/${PORTNAME}" \
		"${STAGE}/etc/inc/priv"

	install -m 0644 "${FILES}${PREFIX}/pkg/ont_monitor.xml" "${STAGE}${PREFIX}/pkg/ont-monitor.xml"
	install -m 0644 "${FILES}${PREFIX}/pkg/ont_monitor.inc" "${STAGE}${PREFIX}/pkg/ont-monitor.inc"
	install -m 0644 "${FILES}${PREFIX}/www/status_ont_monitor.php" "${STAGE}${PREFIX}/www/status_ont_monitor.php"
	install -m 0644 "${FILES}${PREFIX}/www/widgets/widgets/ont_monitor.widget.php" "${STAGE}${PREFIX}/www/widgets/widgets/ont_monitor.widget.php"
	install -m 0644 "${FILES}${PREFIX}/share/${PORTNAME}/info.xml" "${STAGE}${PREFIX}/share/${PORTNAME}/info.xml"
	install -m 0644 "${FILES}/etc/inc/priv/ont_monitor.priv.inc" "${STAGE}/etc/inc/priv/ont_monitor.priv.inc"

	for file in \
		"${STAGE}${PREFIX}/pkg/ont-monitor.xml" \
		"${STAGE}${PREFIX}/share/${PORTNAME}/info.xml"; do
		sed "s/%%PKGVERSION%%/${PORTVERSION}/g" "${file}" > "${file}.tmp"
		mv "${file}.tmp" "${file}"
	done
}

manifest() {
	post_install_script=$(sed "s/%%PORTNAME%%/${PORTNAME}/g" "${FILES}/pkg-install.in" | json_escape)
	pre_deinstall_script=$(sed "s/%%PORTNAME%%/${PORTNAME}/g" "${FILES}/pkg-deinstall.in" | json_escape)

	cat > "${BUILD}/+MANIFEST" <<EOF
name: "${PORTNAME}"
version: "${PORTVERSION}"
origin: "net/${PORTNAME}"
comment: "ZTE F6005 ONT monitor for pfSense"
maintainer: "noreply@github.com"
prefix: "${PREFIX}"
abi: "${ABI}"
desc: "pfSense dashboard widget and status page for ZTE F6005 ONT device, GPON, and Ethernet metrics."
www: "https://github.com/r0bb10/pfSense-ONT-Monitor"
licenselogic: "single"
licenses: ["MIT"]
categories: ["net"]
scripts: {
  post-install: "${post_install_script}",
  pre-deinstall: "${pre_deinstall_script}"
}
EOF

	sed "s|%%DATADIR%%|share/${PORTNAME}|g" "${ROOT}/pkg-plist" > "${BUILD}/plist"
}

package() {
	stage
	manifest
	mkdir -p "${OUTPUT}"
	pkg create -M "${BUILD}/+MANIFEST" -p "${BUILD}/plist" -r "${STAGE}" -o "${OUTPUT}"
	find "${OUTPUT}" -maxdepth 1 -type f -print
}

case "${1:-package}" in
	clean) clean ;;
	stage) stage ;;
	package) package ;;
	*) echo "Usage: $0 [package|stage|clean]" >&2; exit 2 ;;
esac
