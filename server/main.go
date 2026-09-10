// Package main 薄入口：业务请实现 framework.Module 并在 internal/app 注册。
//
//	go run .
//	go run . -config server.yaml
package main

import (
	"log"
	"os"

	"tray_demo/server/internal/app"
)

func main() {
	if err := app.Run(os.Args[1:]); err != nil {
		log.Fatal(err)
	}
}
