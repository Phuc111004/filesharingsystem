-- File: database/schema.sql
-- Corrected schema for File Sharing System

CREATE DATABASE IF NOT EXISTS `file_sharing` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `file_sharing`;

-- USERS
CREATE TABLE IF NOT EXISTS `users` (
	`user_id` INT NOT NULL AUTO_INCREMENT,
	`username` VARCHAR(50) NOT NULL,
	`password` VARCHAR(255) NOT NULL,
	`created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`session_token` VARCHAR(64) DEFAULT NULL,
	`token_expiry` TIMESTAMP NULL DEFAULT NULL,
	`quota` BIGINT NOT NULL DEFAULT 0,
	PRIMARY KEY (`user_id`),
	UNIQUE KEY `ux_users_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- GROUPS
CREATE TABLE IF NOT EXISTS `groups` (
	`group_id` INT NOT NULL AUTO_INCREMENT,
	`group_name` VARCHAR(100) NOT NULL,
	`description` TEXT DEFAULT NULL,
	`created_by` INT NOT NULL,
	`root_dir_id` INT DEFAULT NULL,
	`created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`group_id`),
	KEY `idx_groups_created_by` (`created_by`),
	CONSTRAINT `fk_groups_created_by` FOREIGN KEY (`created_by`) REFERENCES `users`(`user_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- USER_GROUPS (composite PK)
CREATE TABLE IF NOT EXISTS `user_groups` (
	`user_id` INT NOT NULL,
	`group_id` INT NOT NULL,
	`role` ENUM('member','admin') NOT NULL DEFAULT 'member',
	`joined_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`user_id`,`group_id`),
	KEY `idx_user_groups_group_id` (`group_id`),
	CONSTRAINT `fk_user_groups_user` FOREIGN KEY (`user_id`) REFERENCES `users`(`user_id`) ON DELETE CASCADE,
	CONSTRAINT `fk_user_groups_group` FOREIGN KEY (`group_id`) REFERENCES `groups`(`group_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- GROUP REQUESTS
CREATE TABLE IF NOT EXISTS `group_requests` (
	`request_id` INT NOT NULL AUTO_INCREMENT,
	`user_id` INT NOT NULL,
	`group_id` INT NOT NULL,
	`request_type` ENUM('join_request','invitation') NOT NULL,
	`status` ENUM('pending','accepted','rejected') NOT NULL DEFAULT 'pending',
	`created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`request_id`),
	KEY `idx_group_requests_user` (`user_id`),
	KEY `idx_group_requests_group` (`group_id`),
	CONSTRAINT `fk_group_requests_user` FOREIGN KEY (`user_id`) REFERENCES `users`(`user_id`) ON DELETE CASCADE,
	CONSTRAINT `fk_group_requests_group` FOREIGN KEY (`group_id`) REFERENCES `groups`(`group_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- ACTIVITY LOG
CREATE TABLE IF NOT EXISTS `activity_log` (
	`log_id` INT NOT NULL AUTO_INCREMENT,
	`user_id` INT DEFAULT NULL,
	`description` TEXT NOT NULL,
	`created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`log_id`),
	KEY `idx_activity_log_user` (`user_id`),
	CONSTRAINT `fk_activity_log_user` FOREIGN KEY (`user_id`) REFERENCES `users`(`user_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- ROOT DIRECTORY / FILE METADATA
CREATE TABLE IF NOT EXISTS `root_directory` (
	`id` INT NOT NULL AUTO_INCREMENT,
	`group_id` INT NOT NULL,
	`name` VARCHAR(255) NOT NULL,
	`path` TEXT NOT NULL,
	`size` BIGINT NOT NULL DEFAULT 0,
	`uploaded_by` INT DEFAULT NULL,
	`is_folder` BOOLEAN NOT NULL DEFAULT FALSE,
	`parent_id` INT DEFAULT NULL,
	`is_deleted` BOOLEAN NOT NULL DEFAULT FALSE,
	`uploaded_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_root_directory_group` (`group_id`),
	KEY `idx_root_directory_parent` (`parent_id`),
	CONSTRAINT `fk_root_directory_group` FOREIGN KEY (`group_id`) REFERENCES `groups`(`group_id`) ON DELETE CASCADE,
	CONSTRAINT `fk_root_directory_uploaded_by` FOREIGN KEY (`uploaded_by`) REFERENCES `users`(`user_id`) ON DELETE SET NULL,
	CONSTRAINT `fk_root_directory_parent` FOREIGN KEY (`parent_id`) REFERENCES `root_directory`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- Optional: additional tables (e.g., files, file_versions) can be added later.

