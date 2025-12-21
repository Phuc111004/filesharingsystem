-- Seed data for testing
USE file_sharing;

-- -- Create Users
INSERT INTO users (user_id, username, password) VALUES (1, 'admin_user', 'pass1');
INSERT INTO users (user_id, username, password) VALUES (2, 'normal_user', 'pass2');
INSERT INTO users (user_id, username, password) VALUES (3, 'other_user', 'pass3');

-- -- Create Group (User 1 is creator)
INSERT INTO `groups` (group_id, group_name, created_by) VALUES (1, 'Test Group', 1);

-- -- Add User 1 as Admin of Group 1
INSERT INTO `user_groups` (user_id, group_id, role) VALUES (1, 1, 'admin');

-- Add User 2 as Member of Group 1 (for Kick test)
INSERT INTO `user_groups` (user_id, group_id, role) VALUES (2, 1, 'member');
