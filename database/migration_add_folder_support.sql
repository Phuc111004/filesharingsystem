-- Migration script to add folder support to existing database
-- Run this if you already have an existing database with data

USE file_sharing;

-- Add new columns to root_directory table
ALTER TABLE `root_directory` 
ADD COLUMN `is_folder` BOOLEAN NOT NULL DEFAULT FALSE AFTER `uploaded_by`,
ADD COLUMN `parent_id` INT DEFAULT NULL AFTER `is_folder`,
ADD KEY `idx_root_directory_parent` (`parent_id`),
ADD CONSTRAINT `fk_root_directory_parent` 
    FOREIGN KEY (`parent_id`) 
    REFERENCES `root_directory`(`id`) 
    ON DELETE CASCADE;

-- Update existing records: all current entries are files (is_folder = FALSE)
-- This is already the default, but making it explicit
UPDATE `root_directory` SET `is_folder` = FALSE WHERE `is_folder` IS NULL;

-- Optional: Create root folders for each group
-- Uncomment the following if you want a root folder for each group
/*
INSERT INTO `root_directory` (group_id, name, path, size, uploaded_by, is_folder, parent_id)
SELECT 
    g.group_id,
    'Root',
    CONCAT('storage/', g.group_name),
    0,
    g.created_by,
    TRUE,
    NULL
FROM `groups` g
WHERE NOT EXISTS (
    SELECT 1 FROM `root_directory` rd 
    WHERE rd.group_id = g.group_id 
    AND rd.is_folder = TRUE 
    AND rd.parent_id IS NULL
);
*/

SELECT 'Migration completed successfully!' AS Status;
