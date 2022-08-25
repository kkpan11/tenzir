resource "aws_iam_role" "ecs_task_role" {
  name = "${module.env.module_name}-${local.name}-${module.env.stage}-${var.region_name}"

  assume_role_policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "",
      "Effect": "Allow",
      "Principal": {
        "Service": "ecs-tasks.amazonaws.com"
      },
      "Action": "sts:AssumeRole"
    }
  ]
}
EOF
}


resource "aws_iam_role_policy" "fargate_task_policy" {
  name = "${module.env.module_name}-${local.name}--${module.env.stage}-${var.region_name}"
  role = aws_iam_role.ecs_task_role.id

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": ["sqs:SendMessage"],
      "Resource": "${aws_sqs_queue.matched_events.arn}"
    }
  ]
}
EOF
}